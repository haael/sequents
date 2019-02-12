

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <thread>
//#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace execinfo
{
#include <execinfo.h>
};


using std::cerr;
using std::cout;
using std::endl;
using std::set_terminate;
using std::exception;


#define DEBUG

#include "collections.hh"
#include "errors.hh"
#include "formula.hh"
#include "sequent.hh"
#include "sync.hh"
#include "unionfind.hh"

using namespace Logical;

volatile atomic_size_t Logical::max_thread_count(std::thread::hardware_concurrency() * 2);
//volatile atomic_size_t Logical::max_thread_count(1);
volatile sig_atomic_t Logical::thread_error(false);

extern "C" void signal_received(int sig_num)
{
	Logical::thread_error = true;
}

#ifdef DEBUG
static constexpr size_t segfault_stack_max = 256;
void* segfault_stack[segfault_stack_max];
#endif

extern "C" void segfault_received(int sig_num)
{
	Logical::thread_error = true;

#ifdef DEBUG
	size_t stack_size = execinfo::backtrace(segfault_stack, segfault_stack_max);
	char** stack_trace = execinfo::backtrace_symbols(segfault_stack, stack_size);
	for(size_t i = 0; i < stack_size; i++)
		cout << " " << stack_trace[i] << endl;
#endif

	abort();
}

#ifdef DEBUG
mutex Formula::active_objects_mutex;
unordered_set<const Formula*> Formula::active_objects = unordered_set<const Formula*>();
#endif

int main(int argc, char* argv[])
{
	signal(SIGTERM, signal_received);
	signal(SIGABRT, signal_received);
	signal(SIGSEGV, segfault_received);

	try
	{
		cout << "max thread count (0 = unlimited): " << max_thread_count << endl;
		//cout << "sync_test" << endl;
		//sync_test();

		cout << "collections_test" << endl;
		collections_test();

		cout << "unionfind_test" << endl;
		unionfind_test();

		cout << "expression_test" << endl;
		expression_test();
		
		#ifdef DEBUG
		logical_assert(Formula::active_objects.empty());
		#endif
		
		cout << "formula_test" << endl;
		formula_test();
		
		#ifdef DEBUG
		logical_assert(Formula::active_objects.empty());
		#endif
		
		cout << "sequent_test" << endl;
		sequent_test();
		
		#ifdef DEBUG
		logical_assert(Formula::active_objects.empty());
		#endif
	}
	catch(const AssertionError& error)
	{
		cout << error.file << ":" << error.line << ": AssertionError " << error.message << endl;
		// char** stack_trace = execinfo::backtrace_symbols(error.stack, error.stack_size);
		// for(size_t i = 0; i < error.stack_size; i++)
		//	cout << " " << stack_trace[i] << endl;
	}
	catch(const Error& error)
	{
		cout << "Error " << error.message << endl;
		char** stack_trace = execinfo::backtrace_symbols(error.stack, error.stack_size);
		for(size_t i = 0; i < error.stack_size; i++)
			cout << " " << stack_trace[i] << endl;
	}
	catch(const exception& error)
	{
		cout << "System exception: " << error.what() << endl;
	}
	catch(...)
	{
		cout << "Unknown exception" << endl;
	}

	return 0;
}
