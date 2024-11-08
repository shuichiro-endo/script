# script

script

## Build
1. download files
```
git clone https://github.com/shuichiro-endo/script.git
```
2. compile
```
cd script
gcc script.c -o script
```

## Usage
- run
```
cd script
./script
```

- example
```
$ ./script
$ sudo ls
[sudo] password for test: 
README.md  script  script.c  script.h
$ exit

$ cd script
$ ls -la
total 48
drwxr-x---  2 test test  4096 Nov  2 12:26 .
drwxr-xr-x 14 test test  4096 Nov  2 12:21 ..
-rw-r--r--  1 test test   129 Nov  2 12:26 .2433.txt
-rwxr-x---  1 test test  1701 Nov  2 12:21 README.md
-rwxr-xr-x  1 test test 17208 Nov  2 12:22 script
-rwxr-x---  1 test test  8053 Nov  2 12:21 script.c
-rwxr-x---  1 test test    67 Nov  2 12:21 script.h
$ cat .2433.txt
$ ssuuddoo  llss
[sudo] password for test: hypersupersecretpassword
README.md  script  script.c  script.h
$ eexxiitt
$ hexdump -C .2433.txt
00000000  24 20 73 73 75 75 64 64  6f 6f 20 20 6c 6c 73 73  |$ ssuuddoo  llss|
00000010  0d 0d 0a 5b 73 75 64 6f  5d 20 70 61 73 73 77 6f  |...[sudo] passwo|
00000020  72 64 20 66 6f 72 20 74  65 73 74 3a 20 68 79 70  |rd for test: hyp|
00000030  65 72 7f 7f 7f 7f 7f 73  75 70 65 72 73 65 63 72  |er.....supersecr|
00000040  65 74 70 61 73 73 77 6f  72 64 0d 0d 0a 52 45 41  |etpassword...REA|
00000050  44 4d 45 2e 6d 64 20 20  73 63 72 69 70 74 20 20  |DME.md  script  |
00000060  73 63 72 69 70 74 2e 63  20 20 73 63 72 69 70 74  |script.c  script|
00000070  2e 68 0d 0a 24 20 65 65  78 78 69 69 74 74 0d 0d  |.h..$ eexxiitt..|
00000080  0a                                                |.|
00000081
$ 
```

## Notes
### How to run my script program when starting a terminal
- ~/.bashrc
  - sample1
  ```
  trap "rm -rf /tmp/.foobar; exit;" SIGHUP
  if [ ! -f /tmp/.foobar ]; then
      touch /tmp/.foobar
      ~/script/script    # my script program
  fi
  ```
  - sample2
  ```
  result=`pstree -p -t -T | grep $$ | grep "script"`
  if [ -z "$result" ]; then
      ~/script/script    # my script program
  fi
  ```

### How to change logfile directory
1. modify script.c file
```
static char *logfile_directory = "/tmp";
```
2. compile
```
cd script
gcc script.c -o script
```

## License
This project is licensed under the MIT License.

See the [LICENSE](https://github.com/shuichiro-endo/script/blob/main/LICENSE) file for details.
