#FileSpeed

Windows terminal command - 
Measure file i/o speed. 

Visit home website
[https://landenlabs.com](https://landenlabs.com)


Help Banner:
<pre>
FileSpeed v2.7   - Dec 25 2024

  -D                ; generate directory listing
   -o <file>        ; save output to file, null disables output
   -x <num>         ; maximum files to output

  -F                ; measure File read performance
   -a <#pieces>     ; random access, use with -m
   -b <blockSizeKB> ; #KiloBytes to read per transaction, 0=Max which is 8000
   -d s|r           ; open files with hint Sequencial or Random access, default is none
   -h               ; disable file sharing in OPEN call
   -i <infile>      ; file contains list of files to read
   -k <#files>      ; keep #files open during scan, default is 0
   -l <csvFile>     ; save results in CSV log file
   -m <maxMB>       ; maximum Megabytes to read per file, 0=entire file
   -r <repeat#>     ; repeat test
   -s <seconds>     ; time limit of test in seconds, 0=forever till all files read
   -t <seconds>     ; have often to report performance, 0=once at end, default is 10
   -u               ; unbuffered i/o, only works on local file access, bypass file cache
   -w <waitMinutes> ; if Repeat >1, wait between runs

  -C                ; Measure Cache file size
   -c min,max,step; Megabyte range, default is 20,200,20

  -S               ; Dump NTFS Statistics

  -L               ; List Open Files
    -p processName ;   Limit file list to specified processName
    -y s|m|a       ;   Report s=summary, m=modules, a=all
    -r <repeat#>   ;   Repeat output after waitMinutes
    -w <waitMin>   ;   WaitMinutes defaults to 0.1


Example:
  Generate directory file list
   filespeed -D -x 1000 -o filelist.txt d:\wxdata\image\*.wimg
   filespeed -D -x 1000 -o null d:\wxdata\image

  Performance Read test
   filespeed -F -i filelist.txt
   filespeed -F -x 1000 -s 60 d:\wxdata\image\*.dat
   filespeed -F -w 10 -r  6 -i filelist.txt    ; wait 10 minutes, repeat 6 times

  Measure File Cache Size
   filespeed -C  ; default
   filespeed -C -c 1000,2600,100 d:\file1.dat d:\file2.dat   ; specify cacge suze & files
   
</pre>
