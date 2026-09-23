readelf -s kernel/kernel.elf | awk '$4 == "FUNC" && $2 != "00000000" {print $2, $8}' | sort > symbol_dump.txt
