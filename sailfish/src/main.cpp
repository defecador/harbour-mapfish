// The executable must not own any thread-local storage. glibc puts an executable's TLS right
// after the thread control block, where Android's GPU drivers (loaded through libhybris) keep
// their per-thread GL state, and both corrupt each other. CoMaps uses thread_local, so all of it
// lives in libcomaps.so, whose TLS lands after the padding libhybris reserves.
int ComapsMain(int argc, char * argv[]);

int main(int argc, char * argv[])
{
  return ComapsMain(argc, argv);
}
