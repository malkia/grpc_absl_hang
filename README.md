# Issue: Rare hang in `gRPC` when using `abseil` sync primitives on Windows

A rare hang, roughly 1 in ~3000, is observed on Windows 10 and 11 machines, where `gRPC` hangs when using the `abseil` sync primitives.

## Steps to reproduce

```sh
Q:\p\m\grpc_absl_hang>bazel test hang --runs_per_test=10000
INFO: Invocation ID: 3b8fd6ab-2657-474d-933f-3e20e374b6ce
INFO: Analyzed target //:hang (0 packages loaded, 4 targets configured).
[15,017 / 15,020] 3 actions running
    Testing //:hang (run 1295 of 10000); 2299s local
    Testing //:hang (run 8549 of 10000); 1721s local
    Testing //:hang (run 9108 of 10000); 1553s local
```

Except the three hangs above, all other tests have finished in few seconds.

Below is the callstack, after attaching to Visual Studio and debug-breaking in.
Only one thread (main) is present:

```
 	ntdll.dll!NtDelayExecution()	Unknown	Symbols loaded.
 	ntdll.dll!RtlDelayExecution()	Unknown	Symbols loaded.
 	KernelBase.dll!SleepEx()	Unknown	Symbols loaded.
>	hang.exe!AbslInternalSpinLockDelay_lts_20240722(std::atomic<unsigned int> * __formal=0x00007ff6c45a92a0, unsigned int __formal=0x00000009, int loop=0x00008878, absl::lts_20240722::base_internal::SchedulingMode __formal=SCHEDULE_KERNEL_ONLY) Line 32	C++	Symbols loaded.
 	hang.exe!absl::lts_20240722::base_internal::SpinLockDelay(std::atomic<unsigned int> * w=0x00007ff6c45a92a0, unsigned int value=0x00000009, int loop=0x00008878, absl::lts_20240722::base_internal::SchedulingMode scheduling_mode=SCHEDULE_KERNEL_ONLY) Line 91	C++	Symbols loaded.
 	hang.exe!absl::lts_20240722::base_internal::SpinLock::SlowLock() Line 167	C++	Symbols loaded.
 	hang.exe!absl::lts_20240722::base_internal::SpinLock::Lock() Line 83	C++	Symbols loaded.
 	hang.exe!absl::lts_20240722::base_internal::`anonymous namespace'::ArenaLock::ArenaLock(absl::lts_20240722::base_internal::LowLevelAlloc::Arena * arena=0x00007ff6c45a92a0) Line 292	C++	Symbols loaded.
 	hang.exe!absl::lts_20240722::base_internal::LowLevelAlloc::Free(void * v=0x0000028b412c02a0) Line 513	C++	Symbols loaded.
 	hang.exe!absl::lts_20240722::synchronization_internal::ReclaimThreadIdentity(void * v=0x0000028b412c0100) Line 50	C++	Symbols loaded.
 	hang.exe!std::unique_ptr<absl::lts_20240722::base_internal::ThreadIdentity,void (__cdecl*)(void *)>::~unique_ptr<absl::lts_20240722::base_internal::ThreadIdentity,void (__cdecl*)(void *)>() Line 3409	C++	Symbols loaded.
 	hang.exe!`absl::lts_20240722::base_internal::SetCurrentThreadIdentity'::`2'::`dynamic atexit destructor for 'holder''()	C++	Symbols loaded.
 	hang.exe!__dyn_tls_dtor(void * __formal=0x000000007ffe0384, const unsigned long dwReason, void * __formal) Line 119	C++	Symbols loaded.
 	ntdll.dll!LdrpCallInitRoutine()	Unknown	Symbols loaded.
 	ntdll.dll!LdrpCallTlsInitializers()	Unknown	Symbols loaded.
 	ntdll.dll!LdrShutdownProcess()	Unknown	Symbols loaded.
 	ntdll.dll!RtlExitUserProcess()	Unknown	Symbols loaded.
 	kernel32.dll!ExitProcessImplementation()	Unknown	Symbols loaded.
 	ucrtbase.dll!common_exit()	Unknown	Symbols loaded.
 	hang.exe!main(int argc=0x00000001, const char * * argv=0x0000028b40c891b0) Line 49	C++	Symbols loaded.
 	hang.exe!__scrt_common_main_seh() Line 288	C++	Symbols loaded.
 	kernel32.dll!BaseThreadInitThunk()	Unknown	Symbols loaded.
 	ntdll.dll!RtlUserThreadStart()	Unknown	Symbols loaded.
```

## Potential fix

After spending some times digging in `opentelemetry-cpp`, `gRPC` and `abseil` github issues, and then reading the code, 
I've found way to disable the `abseil` primitives, which seems to fix the problem.

```sh 
Q:\p\m\grpc_absl_hang>bazel test hang --runs_per_test=100000 --config=bugfix
```

**No hangs!**

### Patch for the bugfix

This [patch](grpc_no_absl_sync.patch) allows us to disable the `abseil` sync primitives:

```patch
diff --git a/include/grpc/support/port_platform.h b/include/grpc/support/port_platform.h
index 04a79ef3c8..6db7822131 100644
--- a/include/grpc/support/port_platform.h
+++ b/include/grpc/support/port_platform.h
@@ -30,7 +30,7 @@
  * Defines GPR_ABSEIL_SYNC to use synchronization features from Abseil
  */
 #ifndef GPR_ABSEIL_SYNC
-#if defined(__APPLE__)
+#if defined(__APPLE__) || defined(GRPC_NO_ABSL_SYNC)
 // This is disabled on Apple platforms because macos/grpc_basictests_c_cpp
 // fails with this. https://github.com/grpc/grpc/issues/23661
 #else
```

### Testing back-and-forth

Please look in [.bazelrc](.bazelrc) for the bugfix bazel config.

```sh
build:bugfix --platform_suffix=bugfix
build:bugfix --copt="-DGRPC_NO_ABSL_SYNC"
build:bugfix --host_copt="-DGRPC_NO_ABSL_SYNC"
```
