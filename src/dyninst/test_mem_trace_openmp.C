/*
 * See the dyninst/COPYRIGHT file for copyright information.
 *
 * We provide the Paradyn Tools (below described as "Paradyn")
 * on an AS IS basis, and do not warrant its validity or performance.
 * We reserve the right to update, modify, or discontinue this
 * software at any time.  We shall have no obligation to supply such
 * updates or modifications or any other form of support to you.
 *
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

// $Id: test_mem_trace_openmp.C, 2019/10/05 yg31 $
/*
 *
 * #Name: test_mem_trace_openmp
 * #Desc: Instrument all memory accesses by inserting `printf`
 * #Arch: all
 * #Dep:
 */
#include <vector>

#include "BPatch.h"
#include "BPatch_point.h"
#include "BPatch_snippet.h"
#include "BPatch_thread.h"
#include "BPatch_Vector.h"

#include "dyninst_comp.h"
#include "test_lib.h"

#define MODULE_NAME_LENGTH 256

class TestMemTraceOmpMutator : public DyninstMutator {
public:
  virtual test_results_t executeTest();
private:
  BPatch_Vector<BPatch_function*> getFunctionsVector();
  void instrumentMemoryAccess(const BPatch_Vector<BPatch_function*>&vec);
  void insertSnippet(BPatch_Vector<BPatch_point*>* pointsVecPtr, 
                     BPatch_function* function);
};

// Factory function.
extern "C" DLLEXPORT TestMutator* testMemTraceOmpFactory() {
  return new TestMemTraceOmpMutator();
}


/*
 * Return a vector of BPatch_functions* from all modules in an image.
 */
BPatch_Vector<BPatch_function*> TestMemTraceOmpMutator::getFunctionsVector() {
  BPatch_Vector<BPatch_function*> funcVec;
  auto appModules = appImage->getModules();
  if (!appModules) {
    dprintf("Cannot get modules\n");
  }
  char nameBuffer[MODULE_NAME_LENGTH];
  for (auto& module : *appModules) {
    dprintf("Module name: %s\n", 
            module->getFullName(nameBuffer, MODULE_NAME_LENGTH));
    auto procedures = module->getProcedures();
    for (auto& procedure : *procedures) {
      funcVec.push_back(procedure);
    }
  }
  return funcVec;
}

/*
 * Insert `printf` before each memory access point. 
 */
void TestMemTraceOmpMutator::insertSnippet(
                               BPatch_Vector<BPatch_point*>* pointsVecPtr,
                               BPatch_function* function) {
  for (const auto& point : *pointsVecPtr) {
    auto memoryAccess = point->getMemoryAccess();
    if (!memoryAccess) {
      logerror("%s[%d]: null memory access", __FILE__, __LINE__);
      continue;
    }
    if (memoryAccess->isAPrefetch_NP()) {
      logstatus("%s[%d]: current point is a prefetch", __FILE__, __LINE__);
      continue;
    }
    std::vector<BPatch_snippet*> funcArgs;
    funcArgs.push_back(new BPatch_constExpr("Access at: %p. bytes: %u\n"));
    funcArgs.push_back(new BPatch_effectiveAddressExpr());
    funcArgs.push_back(new BPatch_bytesAccessedExpr());
    std::vector<BPatch_function*> printfFuncs;
    appImage->findFunction("printf", printfFuncs);
    BPatch_funcCallExpr printfCall(*(printfFuncs[0]), funcArgs);
    appAddrSpace->insertSnippet(printfCall, *point, BPatch_callBefore);
  } 
}

/*
 * For each function, find load/store points; Then for each load/store point, 
 * insert code snippet at the point.
 */
void TestMemTraceOmpMutator::instrumentMemoryAccess(
        const BPatch_Vector<BPatch_function*>& funcVec) {
  BPatch_Set<BPatch_opCode> opcodes;
  opcodes.insert(BPatch_opLoad);
  opcodes.insert(BPatch_opStore);
  appAddrSpace->beginInsertionSet();
  for (const auto& function : funcVec) {
    auto pointsVecPtr = function->findPoint(opcodes);
    if (!pointsVecPtr) {
      logstatus("%s[%d]: no load/store points for function: %s\n", __FILE__,
              __LINE__, function->getName().c_str());
      continue;
    } else if (pointsVecPtr->size() == 0) {
      logstatus("%s[%d]: load/store points vector is empty for function %s\n", 
                __FILE__, __LINE__, function->getName().c_str());
      continue;
    }
    insertSnippet(pointsVecPtr, function);
  }
}

/*
 * Start Test Case test_memory_tracing_openmp
 */
test_results_t TestMemTraceOmpMutator::executeTest() {
  const char *testName = "Instrument all memory accesses with an empty function";
  auto functions = getFunctionsVector();
  if (functions.size() == 0) {
    logerror("%s[%d]:  No functions found in binary. Treating as test success.\n",
                 __FILE__, __LINE__);
    return PASSED;
  }
  instrumentMemoryAccess(functions);
  appAddrSpace->finalizeInsertionSet(false); 
  dprintf("Instrumented all memory accesses.\n");
  return PASSED;
}
