How to build and run:
1. If not done so, first mount disk.img to /mnt/C (mount.sh does this if you want to run it)
2. build.sh will build the os (called memos-2, but it is really micros), and will flush the disk (umount then remount)
3. optionally you can run flush.sh yourself to flush the disk
4. use run.sh to run qemu and open vncviewer

I refered to osdev.org for their PIC, PIT, GDT and IDT tutorials
I started with the memos-2 starter code provided by the assigment.

I have 3 main messages printed for the operating system: (NOTE: a time slice the amount of time the timer takes to interrupt, eg. if the timers Hz was 100, a timeslice would be 10ms)
1. "CS: XX to YY S: ZZZ" This is saying there will be a context switch from TID XX to TID YY at time slice ZZ. This is printed in scheduler.c:scheduler_context_switch()
2. "<XX,YY,ZZ>" this is outputed in test_funcs.c:test_write_final(), the test thread function. XX is the TID, YY is the cost Ci, and ZZ is Ti. The function only writes this string once per time slice, so after writing the function will busy wait until the next time slice its running.
3. "TID: XX L: YY slices" this prints at the end of the thread function and says that TID XX ran for YY time slices.

If you need to kill a qemu process for any reason, use ./kill.sh
