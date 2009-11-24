#include<cstring>
#include<cstdlib>
#include<iostream>
#include<sstream>
#include<string>
#include<vector>
#include<algorithm>
using namespace std;

#ifndef WIN32
  #define PATH_SEP "/"
  #include<dirent.h>
#else
  #define PATH_SEP "\\"
#include "windirent.h"
#endif


// Return 1 if a file is found with the prefix in the directory given by path.
//******************************************************************************
int Represented(const char *path, const char *prefix)
{
  size_t prefixLen=strlen(prefix);
  #ifndef NDEBUG
  if (prefix[prefixLen-1]!='_')
    {
    cerr << __LINE__ << " Error: prefix is expected to end with '_' but it does not." << endl;
    return 0;
    }
  #endif
  DIR *ds=opendir(path);
  if (ds)
    {
    struct dirent *de;
    while ((de=readdir(ds)))
      {
      char *fname=de->d_name;
      if (strncmp(fname,prefix,prefixLen)==0)
        {
        // Found at least one file beginning with the given prefix.
        closedir(ds);
        return 1;
        }
      }
    closedir(ds);
    }
  else
    {
    #ifndef NDEBUG
    cerr << __LINE__ << " Error: Failed to open the given directory. " << endl
         << path << endl;
    #endif
    }
  //We failed to find any files starting with the given prefix
  return 0;
}

// Collect the ids from a collection of files that start with
// the same prefix (eg. prefix_ID.ext). The prefix should include
// the underscore.
//*****************************************************************************
int GetSeriesIds(const char *path, const char *prefix, vector<int> &ids)
{
  size_t prefixLen=strlen(prefix);
  #ifndef NDEBUG
  if (prefix[prefixLen-1]!='_')
    {
    cerr << __LINE__ << " Error: prefix is expected to end with '_' but it does not." << endl;
    return 0;
    }
  #endif

  DIR *ds=opendir(path);
  if (ds)
    {
    struct dirent *de;
    while ((de=readdir(ds)))
      {
      char *fname=de->d_name;
      if (strncmp(fname,prefix,prefixLen)==0)
        {
        if (isdigit(fname[prefixLen]))
          {
          int id=atoi(fname+prefixLen);
          ids.push_back(id);
          }
        }
      }
    closedir(ds);
    sort(ids.begin(),ids.end());
    return 1;
    }
  else
    {
    #ifndef NDEBUG
    cerr << __LINE__ << " Error: Failed to open the given directory. " << endl
         << path << endl;
    #endif
    }
  //We failed to find any files starting with the given prefix
  return 0;
}


// Returns the path not including the file name and not
// including the final PATH_SEP. If PATH_SEP isn't found 
// then ".PATH_SEP" is returned.
//*****************************************************************************
string StripFileNameFromPath(const string fileName)
{
  size_t p;
  p=fileName.find_last_of(PATH_SEP);
  if (p==string::npos)
    {
    // current directory
    return "." PATH_SEP;
    }
  return fileName.substr(0,p); // TODO Why does this leak?
}


