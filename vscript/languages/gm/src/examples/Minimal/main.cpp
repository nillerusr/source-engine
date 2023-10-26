#if 0 // This is trully the smallest app

#include "gmThread.h" // game monkey script

int main(int argc, char* argv[])
{
  gmMachine machine;
  machine.ExecuteString("print(`Hello world`);");
  getchar(); // Keypress before exit
  return 0;
}

#else // This is a tiny app

#include <windows.h>  
#include <mmsystem.h> // multimedia timer (may need winmm.lib)
#include "gmThread.h" // game monkey script

int main(int argc, char* argv[])
{
  // Create virtual machine
  gmMachine* machine = new gmMachine;

  // Get a script from stdin.  Some examples:
  // print("Hello world");
  // for( i = 0; i < 10; i=i+1 ) { print("i=",i); sleep(1.0); }
  fprintf(stdout,"Please enter one line of script\n>");
  const int MAX_SCRIPT_SIZE = 4096;
  char script[MAX_SCRIPT_SIZE];
  fgets(script, MAX_SCRIPT_SIZE-1, stdin);
  
  // Compile the script, but don't run it for now
  int errors = machine->ExecuteString(script, NULL, false, NULL);
  // Dump compile time errors to output
  if(errors)
  {
    bool first = true;
    const char * message;
    
    while((message = machine->GetLog().GetEntry(first))) 
    {
      fprintf(stderr, "%s"GM_NL, message);
    }
    machine->GetLog().Reset();
  }
  else
  {
    int deltaTime = 0;
    int lastTime = timeGetTime();
    
    // Keep executing script while threads persist
    while(machine->Execute(deltaTime))
    {
      // Update delta time
      int curTime = timeGetTime();
      deltaTime = curTime - lastTime;
      lastTime = curTime;

      // Dump run time errors to output
      bool first = true;
      const char * message;
      while((message = machine->GetLog().GetEntry(first))) 
      {
        fprintf(stderr, "%s"GM_NL, message);
      }
      machine->GetLog().Reset();
    }
  }

  delete machine; // Finished with VM
      
  fprintf(stdout,"Script complete.  Press a key to exit.");
  getchar(); // Keypress before exit
  
  return 0;
}

#endif // Minimal build type