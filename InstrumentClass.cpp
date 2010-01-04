#include <string>
using std::string;
#include <vector>
using std::vector;
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include "FsUtils.h"

int main(int argc, char **argv)
{
  if (argc!=2)
    {
    cerr << "Error exiting." << endl
         << "Usage:" << endl
         << "    " << argv[0] << " /path/to/class.cxx" << endl
         << endl;
    return 1;
    }

  string classFile(argv[1]);

  string className;
  className=StripExtensionFromFileName(StripPathFromFileName(classFile));

  string classText;
  if (!LoadText(classFile,classText))
    {
    cerr << "Failed to read the file " << classFile << ". Exiting." << endl
         << endl;
    return 1;
    }

  string methodKey(className);
  methodKey+="::";

  size_t pos=0;
  size_t end=classText.size();

  vector<size_t> methodLoc;
  vector<string> methodName;
  size_t nMethods=0;

  // first identify all methods
  while ((pos=classText.find(methodKey,pos))!=string::npos)
    {
    // pos points to the posible method name, now we have to find the opening
    // of the method, if its a method declaration not a static method call.
    size_t nextSemi=classText.find(";",pos);
    size_t nextCurly=classText.find("{",pos);
    size_t nextParen=classText.find("(",pos);
    // In any case there must be a open curly for the method.
    if (nextCurly==string::npos || nextParen==string::npos)
      {
      cerr
        << "Syntax error no " << (nextCurly==string::npos?"{":"(") 
        << " found after position " << pos
        << endl
        << endl;
      return 1;
      }
   // If this is not a static method call then  open curly occurs before the next semi.
   if (nextSemi!=string::npos && nextSemi<nextCurly)
    {
    // static method call.
    pos=nextSemi;
    continue;
    }
   // If this is not member class call then open paren occurs before the next semi.
   if (nextSemi!=string::npos && nextSemi<nextParen)
    {
    // member class
    pos=nextSemi;
    continue;
    }

    ++nMethods;
    methodLoc.push_back(nextCurly+1);
    methodName.push_back(classText.substr(pos+methodKey.size(),nextParen-pos-methodKey.size()));

    pos=nextCurly;
    }

  // now we have a list of method names and their locations, pass the
  // class text to the output and insert the instrumentation code.
  size_t lastPos=0;
  for (size_t i=0; i<nMethods; ++i)
    {
    size_t currentPos=methodLoc[i];
    cout << classText.substr(lastPos,currentPos-lastPos)
         << endl
         << "  #ifdef " << className << "DEBUG" << endl
         << "    cerr << \"=====================================================" << methodKey << methodName[i] << "\" << endl;" << endl
         << "  #endif" << endl
         << endl;
    lastPos=currentPos;
    }
  cout << classText.substr(lastPos,string::npos) << endl;

  return 0;
}
