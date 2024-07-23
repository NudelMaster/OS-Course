--------------------------------
| Operating Systems ex3 tester |
--------------------------------

How to use
----------
* Place ex3_tester in the folder of the kernel module (with the message_slot.c, message_slot.h files).
* Run the following commands:

$ sudo mknod /dev/test0 c 240 0
$ sudo mknod /dev/test1 c 240 1
$ sudo chmod 0777 /dev/test0
$ sudo chmod 0777 /dev/test1
$ gcc -O3 -Wall -std=c11 ex3_tester.c -o tester
$ ./tester


About the tester
----------------
The tester is based of testing description file from previous semesters.
There are 14 tests in total, which test the kernel module (message_slot.c) only.
Note: ex3_tester.c does NOT test message_sender.c and message_reader.c.



Good luck :)
