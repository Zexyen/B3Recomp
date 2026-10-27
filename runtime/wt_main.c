/* witch-time-game: the runtime is libwitchtime.so beside this program (boot.c wt_host_main), so the whole of it can be
 * prebuilt; the launcher starts this when it cannot load the library itself. */
int wt_host_main(int argc, char **argv);
int main(int argc, char **argv) { return wt_host_main(argc, argv); }
