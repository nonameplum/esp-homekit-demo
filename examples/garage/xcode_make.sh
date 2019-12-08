#!/bin/bash
PATH="/Volumes/esp8266/esp-open-sdk/xtensa-lx106-elf/bin:$PATH"
export SDK_PATH="/Volumes/esp8266/esp-open-rtos"
export PATH=/bin:/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:$PATH

if [ "$1" == "clean" ]; then
    echo "CLEAN"
    dir=${2%/*}
    dir=${dir%/*}
    dir=${dir%/*}
    
    build_dir="$dir/build/"
    echo 'Clean build' $build_dir
    rm -rf "$build_dir"
    
    build_dir_2="$dir/Build/"
    echo 'Clean Build' $build_dir_2
    rm -rf "$build_dir_2"
    
    output=$((make -C . clean) 2>&1)
    echo "$output"
else
    echo "BUILD"
    
    output=$((make -C . all) 2>&1)

    echo "$output"

    echo "$output" | perl -e '$x = ""; while (<>) { $x .= $_ }; my @arr = (); while ($x =~ /^([^\n]*?) error:((.|\n)*?)note: (.*?)\n/mg) { push @arr, "$1 warning: $4" }; foreach (@arr) { print "$_\n" }'
    echo "$output" | perl -e '$x = ""; while (<>) { $x .= $_ }; my @arr = (); while ($x =~ /^([^\n]*?) fatal error:((.|\n)*?)(.*?)\n/mg) { push @arr, "$1 error: $4" }; foreach (@arr) { print "$_\n" }'


    if [[ $output == *"Creating image"* ]]; then
      exit 0
    fi

    if [[ $output == *"error"* ]]; then
      exit 1
    fi

    exit 0
fi
