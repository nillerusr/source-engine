#include <windows.h>  
#include "StdStuff.h"
#include "App.h"

// Entry point for Win32 app
int main(int argc, char* argv[])
{
  String test1("hello");
  test1 = "world";
  const char* huh = test1;

  App app;

  if(!app.Init())
  {
    fprintf(stderr,"Failed App::Init()");
    return 1;
  }

  while(app.Update())
  {
  }

  app.Destroy();

  printf("App finished, press ENTER to exit.");
  getchar(); // Wait for key press

  return 0;
}
