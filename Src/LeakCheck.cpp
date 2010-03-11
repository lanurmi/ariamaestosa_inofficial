/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Config.h"
#include <ptr_vector.h>

#ifdef _MORE_DEBUG_CHECKS

#define GET_STACK_TRACE 0

#ifdef __WXMAC__
#include <Availability.h>
#include <execinfo.h>
#endif

#include "ptr_vector.h"

#include "LeakCheck.h"
#include <iostream>

namespace AriaMaestosa
{
namespace MemoryLeaks
{

ptr_vector<MyObject> obj;

void addObj(MyObject* myObj)
{
    //std::cout << "addObj " << myObj->file << " (" << myObj->line << ")" << std::endl;
    obj.push_back(myObj);
}

void removeObj(MyObject* myObj)
{
    //std::cout << "removeObj " << myObj->file << " (" << myObj->line << ")" << std::endl;
    obj.remove(myObj);
    delete myObj;
    //std::cout << "removeObj done" << std::endl;
}

MyObject::MyObject(AbstractLeakCheck* obj)
{
    this->obj = obj;
    
#if (GET_STACK_TRACE == 1) && defined(MAC_OS_X_VERSION_10_5)

    const int maxsize = 32;
    void* callstack[maxsize];
    stackSize = backtrace(callstack, maxsize);

    stack = backtrace_symbols(callstack, stackSize);
#else
    stack = NULL;
#endif
    
}

void MyObject::print()
{
    obj->print();
    
    if (stack == NULL)
    {
        printf("    (No stack information available)\n");
    }
    else
    {
        for (int i = 0; i < stackSize; ++i)
        {
            printf("    %s\n", stack[i]);
        }
    }
    
}

void checkForLeaks()
{

    std::cout << "checking for leaks... " << std::endl;

    if (obj.size()>0)
    {
        std::cout << "leaks detected!!" << std::endl;
        std::cout << "\n\n* * * * WARNING * * * * WARNING * * * * MEMORY LEAK! * * * *\n" << std::endl;
    }
    else
    {
        std::cout << "ok (no watched class left leaking)" << std::endl;
        return;
    }
    std::cout << "LEAK CHECK: " << obj.size() << " watched objects leaking" << std::endl;


    for(int n=0; n<obj.size(); n++)
    {
        obj[n].print();
    }
}


}
}
#endif