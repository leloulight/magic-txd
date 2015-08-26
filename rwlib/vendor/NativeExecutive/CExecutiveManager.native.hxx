// File that includes native module definitions for NativeExecutive.

#ifndef _NATIVE_EXECUTIVE_INTERNAL_
#define _NATIVE_EXECUTIVE_INTERNAL_

#if defined(_M_IX86)

// Fiber routines.
extern "C" void __stdcall _fiber86_retHandler( NativeExecutive::FiberStatus *userdata );
extern "C" void __cdecl _fiber86_eswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );
extern "C" void __cdecl _fiber86_qswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );

// Thread routines.
extern "C" DWORD WINAPI _thread86_procNative( LPVOID lpThreadParameter );
extern "C" void __stdcall _thread86_termProto( void );

#elif defined(_M_AMD64)

// Fiber routines.
extern "C" void __cdecl _fiber64_procStart( void );
extern "C" void __cdecl _fiber64_eswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );
extern "C" void __cdecl _fiber64_qswitch( NativeExecutive::Fiber *from, NativeExecutive::Fiber *to );

// Thread routines.
extern "C" DWORD WINAPI _thread64_procNative( LPVOID lpThreadParameter );
extern "C" void _thread64_termProto( void );

#endif

#endif //_NATIVE_EXECUTIVE_INTERNAL_