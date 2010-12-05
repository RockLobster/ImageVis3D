/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/


//!    File   : main.cpp
//!    Author : Jens Krueger
//!             SCI Institute
//!             University of Utah
//!    Date   : December 2008
//
//!    Copyright (C) 2008 SCI Institute

#include "../Tuvok/StdTuvokDefines.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <QtGui/QApplication>
#ifndef TUVOK_NO_QT
  #include <QtGui/QImageReader>
#endif
#include <tclap/CmdLine.h>

#include "../Tuvok/Controller/Controller.h"
#include "../Tuvok/Basics/SysTools.h"
#include "DebugOut/HRConsoleOut.h"
#include "../Tuvok/IO/TuvokIOError.h"
#include "../Tuvok/IO/IOManager.h"
#include "../Tuvok/IO/DirectoryParser.h"

using namespace std;
using namespace tuvok;

/*
#ifdef _WIN32
  // CRT's memory leak detection on windows
  #if defined(DEBUG) || defined(_DEBUG)
    #include <crtdbg.h>
  #endif
#endif
  */

enum {
  EXIT_FAILURE_ARG = 1,       // invalid argument
  EXIT_FAILURE_UNKNOWN_OUT,   // unknown file type for output file
  EXIT_FAILURE_RO_VOL_OUT,    // file known as volume but converter is read only
  EXIT_FAILURE_RO_GEO_OUT,    // file known as mesh but converter is read only
  EXIT_FAILURE_UNKNOWN_1,     // unknown file type for first input file
  EXIT_FAILURE_UNKNOWN_2,     // unknown file type for second file in merge
  EXIT_FAILURE_CROSS_1,       // trying to convert a volume into a mesh
  EXIT_FAILURE_CROSS_2,       // trying to convert a mesh into a volume
  EXIT_FAILURE_MESH_MERGE,    // trying to merge meshes
  EXIT_FAILURE_TO_RAW,        // error during source to raw conversion step 
  EXIT_FAILURE_TO_UVF,        // error during raw to uvf conversion step 
  EXIT_FAILURE_GENERAL,       // general error during conversion (not to UVF)
  EXIT_FAILURE_IN_MESH_LOAD,  // unable to open the input mesh
  EXIT_FAILURE_OUT_MESH_WRITE,// unable to write output mesh
  EXIT_FAILURE_MERGE,         // general error during file merge
  EXIT_FAILURE_DIR_MERGE,     // attempting to merge in directory mode
  EXIT_FAILURE_MERGE_NO_UVF,  // attempting to merge to format other than UVF
  EXIT_FAILURE_GENERAL_DIR,   // general error during conversion in dir mode
  EXIT_FAILURE_NEED_UVF,      // UVFs must be input to eval expressions.
};

// reads an entire file into a string.
static std::string readfile(const std::string& filename)
{
  // open in append mode so the file pointer will be at EOF and we can
  // therefore easily/quickly figure out the file size.
  std::ifstream ifs(filename.c_str(), std::ios::in | std::ios::ate);
  if(!ifs.is_open()) {
    T_ERROR("Could not open file '%s'", filename.c_str());
    return "";
  }
  std::ifstream::pos_type len = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  std::vector<char> contents(len+std::ifstream::pos_type(1), 0);
  size_t offset=0;
  do {
    std::streamsize length = std::streamsize(len) - std::streamsize(offset);
    ifs.read(&contents[offset], length);
    offset += ifs.gcount();
  } while(!ifs.eof() && std::ifstream::pos_type(offset) < len);
  ifs.close();

  return std::string(&contents[0]);
}

int main(int argc, const char* argv[])
{
/*
// Enable run-time memory check for debug builds on windows
  #ifdef _WIN32
    #if defined(DEBUG) | defined(_DEBUG)
      _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
    #endif
  #endif
*/
  std::vector<std::string> input;
  std::string output, directory;
  std::string expression;

  // temp
  string strInFile;
  string strInFile2;
  string strInDir;
  string strOutFile;
  double fScale = 0.0;
  double fBias = 0.0;

  try {
    TCLAP::CmdLine cmd("uvf converter");
    TCLAP::MultiArg<std::string> inputs("i", "input", "input file.  "
                                        "Repeat to merge volumes", true,
                                        "filename");
    TCLAP::ValueArg<std::string> directory("d", "directory",
                                           "input directory", true, "",
                                           "path");
    TCLAP::ValueArg<std::string> expr("e", "expression",
                                      "merge expression", false, "", "string");
    TCLAP::ValueArg<std::string> output("o", "output", "output file (uvf)",
                                        true, "", "filename");
    TCLAP::ValueArg<double> bias("b", "bias",
                                 "(merging) bias value for second file",
                                 false, 0.0, "floating point number");
    TCLAP::ValueArg<double> scale("s", "scale",
                                  "(merging) scaling value for second file",
                                  false, 0.0, "floating point number");
    
    cmd.xorAdd(inputs, directory);
    cmd.add(output);
    cmd.add(bias);
    cmd.add(scale);
    cmd.add(expr);
    cmd.parse(argc, argv);

    // which of "-i" or "-d" did they give?
    if(inputs.isSet()) {
      strInFile = inputs.getValue()[0];
      if(inputs.getValue().size() > 1) {
        strInFile2 = inputs.getValue()[1];
      }
      input = inputs.getValue();
    }
    if(directory.isSet()) {
      strInDir = directory.getValue();
    }
    strOutFile = output.getValue();
    fBias = bias.getValue();
    fScale = scale.getValue();

    if(expr.isSet()) {
      expression = expr.getValue();
      if(SysTools::FileExists(expression)) {
        expression = readfile(expression);
      }
    }
  } catch(const TCLAP::ArgException& e) {
    std::cerr << "error: " << e.error() << " for arg " << e.argId() << "\n";
    return EXIT_FAILURE_ARG;
  }

  HRConsoleOut* debugOut = new HRConsoleOut();
  debugOut->SetOutput(true, true, true, false);
  debugOut->SetClearOldMessage(true);

  Controller::Instance().AddDebugOut(debugOut);
  IOManager ioMan;

  // If they gave us an expression, evaluate that.  Otherwise we're doing a
  // normal conversion.
  if(!expression.empty()) {
    // All the input files need to be UVFs if they're merging volumes.
    for(std::vector<std::string>::const_iterator f = input.begin();
        f != input.end(); ++f) {
      if(ioMan.NeedsConversion(*f)) {
        T_ERROR("Expression evaluation currently requires all input volumes "
                "to be stored as UVFs.");
        return EXIT_FAILURE_NEED_UVF;
      }
    }
    try {
      ioMan.EvaluateExpression(expression.c_str(), input, strOutFile);
    } catch(const std::exception& e) {
      std::cerr << "expr exception: " << e.what() << "\n";
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  // Verify we can actually convert the data.  We can't do this for
  // directories unless we've scanned the directory already, so delay
  // error detection there.
  if(strInDir.empty()) {
    for(std::vector<std::string>::const_iterator f = input.begin();
        f != input.end(); ++f) {
      std::string ext = SysTools::ToLowerCase(SysTools::GetExt(*f));
      bool conv_vol = ioMan.GetConverterForExt(ext, false) != NULL;
      bool conv_geo = ioMan.GetGeoConverterForExt(ext, false) != NULL;
      if(conv_vol || conv_geo) { continue; }

      if(!conv_vol && !conv_geo) {
        T_ERROR("Unknown file type for '%s'", f->c_str());
        return EXIT_FAILURE_UNKNOWN_OUT;
      }
    }
  }

  string targetType = SysTools::ToLowerCase(SysTools::GetExt(strOutFile));
  if (!strInFile.empty()) {
    string sourceType = SysTools::ToLowerCase(SysTools::GetExt(strInFile));

    bool bIsVolExtOut = ioMan.GetConverterForExt(targetType, true) != NULL;
    bool bIsGeoExtOut = ioMan.GetGeoConverterForExt(targetType, true) != NULL;
    bool bIsVolExt1 = ioMan.GetConverterForExt(sourceType, false) != NULL;
    bool bIsGeoExt1 = ioMan.GetGeoConverterForExt(sourceType, false) != NULL;

    if (!bIsVolExt1 && !bIsGeoExt1)  {
      std::cerr << "error: Unknown file type for '" << strInFile << "'\n";
      return EXIT_FAILURE_UNKNOWN_1;
    }

    if (bIsVolExt1 && bIsGeoExtOut)  {
      std::cerr << "error: cannot convert volume to geometry\n";
      return EXIT_FAILURE_CROSS_1;
    }

    if (bIsGeoExt1 && bIsVolExtOut)  {
      std::cerr << "error: cannot convert geometry to volume\n";
      return EXIT_FAILURE_CROSS_2;
    }

    if (strInFile2.empty()) {
      if (bIsVolExt1) {
        if (targetType == "uvf" && sourceType == "uvf") {
          cout << endl << "Running in UVF to UVF mode, "
               << "perserving only the raw data from " << strInFile << " to "
               << strOutFile << endl;

          cout << "Step 1. Extracting raw data" << endl;
          /// use some simple format as intermediate file
          string tmpFile = SysTools::ChangeExt(strOutFile,"nrrd");

          // HACK: use the output file's dir as temp dir
          if (ioMan.ConvertDataset(strInFile, tmpFile,
                                   SysTools::GetPath(tmpFile))) {
            cout << endl << "Success." << endl << endl;
          } else {
            cout << endl << "Extraction failed!" << endl << endl;
            return EXIT_FAILURE_TO_RAW;
          }

          cout << "Step 2. Writing new UVF file" << endl;
          // HACK: use the output file's dir as temp dir
          if (ioMan.ConvertDataset(tmpFile, strOutFile,
                                   SysTools::GetPath(strOutFile))) {
            if(std::remove(tmpFile.c_str()) == -1) {
             cout << endl << "Conversion succeeded but "
                  << " could not delete tmp file " << tmpFile << "\n\n";
            } else {
             cout << "\nSuccess.\n\n";
            }
            return EXIT_SUCCESS;
          } else {
            if(std::remove(tmpFile.c_str()) == -1) {
             cout << "\nUVF write failed and could not delete tmp file "
                  << tmpFile << "\n\n";
            } else {
             cout << "\nUVF write failed.\n\n";
            }
            return EXIT_FAILURE_TO_UVF;
          }
        } else {
          cout << endl << "Running in volume file mode.\nConverting "
               << strInFile << " to " << strOutFile << "\n\n";
          // HACK: use the output file's dir as temp dir
          if (ioMan.ConvertDataset(strInFile, strOutFile,
                                   SysTools::GetPath(strOutFile))) {
            cout << "\nSuccess.\n\n";
            return EXIT_SUCCESS;
          } else {
            cout << "\nConversion failed!\n\n";
            return EXIT_FAILURE_GENERAL;
          }
        }
      } else {
          AbstrGeoConverter* sourceConv = ioMan.GetGeoConverterForExt(sourceType, false);
          AbstrGeoConverter* targetConv = ioMan.GetGeoConverterForExt(targetType, false);

          cout << "\nRunning in geometry file mode.\n"
               << "Converting " << strInFile
               << " (" << sourceConv->GetDesc() << ") to "
               << strOutFile << " (" << targetConv->GetDesc() << ")\n";
          Mesh* m = NULL;
          try {
            m = sourceConv->ConvertToMesh(strInFile);
          } catch (const tuvok::io::DSOpenFailed& err) {
            cerr << "Error trying to open the input mesh "
                 << "(" << err.what() << ")\n";
            return EXIT_FAILURE_IN_MESH_LOAD;
          }
          if (!targetConv->ConvertToNative(*m,strOutFile)) {
            cerr << "Error writing target mesh\n";
            return EXIT_FAILURE_OUT_MESH_WRITE;
          }
          delete m;
      }
    } else {

      string sourceType2 = SysTools::ToLowerCase(SysTools::GetExt(strInFile2));

      bool bIsVolExt2 = ioMan.GetConverterForExt(sourceType2, false) != NULL;
      bool bIsGeoExt2 = ioMan.GetGeoConverterForExt(sourceType2, false) != NULL;

      if (!bIsVolExt2 && !bIsGeoExt2)  {
        std::cerr << "error: Unknown file type for '" << strInFile2 << "'\n";
        return EXIT_FAILURE_UNKNOWN_2;
      }

      if (bIsGeoExt2)   {
        std::cerr << "error: Mesh merge not supported at the moment\n";
        return EXIT_FAILURE_MESH_MERGE;
      }

      vector<string> vDataSets;
      vector<double> vScales;
      vector<double> vBiases;
      vDataSets.push_back(strInFile);
      vScales.push_back(1.0);
      vBiases.push_back(0.0);
      vDataSets.push_back(strInFile2);
      vScales.push_back(fScale);
      vBiases.push_back(fBias);

      cout << endl << "Running in merge mode.\nConverting";
      for (size_t i = 0;i<<vDataSets.size();i++) {
        cout << " " << vDataSets[i];
      }
      cout << " to " << strOutFile << "\n\n";

      // HACK: use the output file's dir as temp dir
      if (ioMan.MergeDatasets(vDataSets, vScales, vBiases, strOutFile,
                              SysTools::GetPath(strOutFile))) {
        cout << "\nSuccess.\n\n";
        return EXIT_SUCCESS;
      } else {
        cout << "\nMerging datasets failed!\n\n";
        return EXIT_FAILURE_MERGE;
      }

    }
  } else {
    if (strInFile2 != "") {
      cout << "\nError: Currently file merging is only supported "
           << " in file mode (i.e. specify -f and not -d).\n\n";
      return EXIT_FAILURE_DIR_MERGE;
    }

    /// \todo: remove this restricition (one solution would be to create a UVF
    // first and then convert it to whatever is needed)
    if (targetType != "uvf") {
      cout << "\nError: Currently only uvf is the only supported "
           << "target type for directory processing.\n\n";
      return EXIT_FAILURE_MERGE_NO_UVF;
    }

    cout << "\nRunning in directory mode.\nConverting "
         << strInDir << " to " << strOutFile << "\n\n";

    vector<std::tr1::shared_ptr<FileStackInfo> > dirinfo =
      ioMan.ScanDirectory(strInDir);

    vector<string> vStrFilenames(dirinfo.size());
    if (dirinfo.size() == 1) {
       vStrFilenames[0] = strOutFile;
    } else {
      string strFilenameAndDirectory = SysTools::RemoveExt(strOutFile);
      // should be "uvf" but we never know what the user specified
      string strExt = SysTools::GetExt(strOutFile);
      for (size_t i = 0;i<dirinfo.size();i++) {
        vStrFilenames[i] = SysTools::AppendFilename(strOutFile, int(i)+1);
      }
    }


    int iFailCount = 0;
    for (size_t i = 0;i<dirinfo.size();i++) {
      if (ioMan.ConvertDataset(&*dirinfo[i], vStrFilenames[i],
                               SysTools::GetPath(vStrFilenames[i]))) {
        cout << "\nSuccess.\n\n";
      } else {
        cout << "\nConversion failed!\n\n";
        iFailCount++;
        return EXIT_FAILURE_GENERAL_DIR;
      }
    }

    if (iFailCount != 0)  {
      cout << endl << iFailCount << " out of " << dirinfo.size()
           << " stacks failed to convert properly.\n\n";
    }

    return EXIT_SUCCESS;
  }
}
