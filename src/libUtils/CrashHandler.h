
#include <stdio.h>
#include <signal.h>
#include <execinfo.h>


static void PrintStackTrace()
{
   std::unique_ptr<void []> array(new void[256]);
   size_t size = backtrace(array, 256);
   std::unique_ptr<char []> strings(new char[256]);
   strings.get() = backtrace_symbols(array, 256);
   /*if (strings)
   {
      printf("--Stack trace follows (%zd frames):\n", size);
      for (size_t i = 0; i < size; i++) printf("  %s\n", strings[i]);
      printf("--End Stack trace\n");
      free(strings);
   }
   else printf("PrintStackTrace:  Error, could not generate stack trace!\n");*/
}


