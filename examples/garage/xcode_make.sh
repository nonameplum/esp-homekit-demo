#!/bin/bash
PATH="/Volumes/esp8266/esp-open-sdk/xtensa-lx106-elf/bin:$PATH"
export SDK_PATH="/Volumes/esp8266/esp-open-rtos"
export PATH=/bin:/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:$PATH

output=$((make -C . all) 2>&1)

echo "$output"

echo "$output" | perl -e '$x = ""; while (<>) { $x .= $_ }; my @arr = (); while ($x =~ /^([^\n]*?) error:((.|\n)*?)note: (.*?)\n/mg) { push @arr, "$1 warning: $4" }; foreach (@arr) { print "$_\n" }'

if [[ $output == *"error"* ]]; then
  exit 1
fi

exit 0
