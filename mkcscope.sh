#!/bin/sh


rm -rf $FS_HOME/.cscope/cscope.files $FS_HOME/.cscope/cscope.files

find $FS_HOME/src/ -name '*.[ch]' > $FS_HOME/.cscope/cscope.files

cscope -b -q -i $FS_HOME/.cscope/cscope.files
mv cscope* .cscope/
