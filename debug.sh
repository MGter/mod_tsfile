conf_name=$1

rm mod_tsfile -f
sh make_file.sh  
gdb  ./mod_tsfile   $conf_name
