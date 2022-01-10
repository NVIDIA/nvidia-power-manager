#!/bin/sh

# Help - psui2ccmd.sh.sh <PSU number> <Command> 
# PSU number range from 0-5

string=''
removenonacii=0
i2c_read() {
    i2cset -f -y $1 0x70 0x00 0x0$(($2 - 0x3c))
    data=$(i2cget -f -y $1 $2 $3 i $4)

    if [[ -z "$data" ]]; then
        # echo "i2c$1 device $2 command $3 error" >&2
        # exit 1
        string='00_00'
        return
    fi
    string=''
    if [ "$3" == "0xe2" ]
    then
        arry=$(echo ${data} | sed -e "s/$4\: //" | sed -e "s/\0x00//g" | sed -e "s/\0xff//g" | sed -e "s/\0x7f//g" | sed -e "s/\0x0f//g" | sed -e "s/\0x14//g")
        for d in ${arry}
        do
            hex=$(echo $d | sed -e "s/0\x//")
            string+=$(echo  "${hex}.");
        done
        string=$(echo ${string} | cut -c10-14)

    else
        removenonacii=1
        arry=$(echo ${data} | sed -e "s/$4\: //" | sed -e "s/\0x00//g" | sed -e "s/\0xff//g" | sed -e "s/\0x7f//g" | sed -e "s/\0x0f//g" | sed -e "s/\0x14//g")
        for d in ${arry}
        do
            hex=$(echo $d | sed -e "s/0\x//")
            string+=$(echo -e "\x${hex}");
        done
    fi
}

if [ $# -le 1 ]; then
    echo 'Usage : psui2ccmd.sh.sh <PSU number> <Command>' >&2
    exit 1
fi

psu_number=$((0x40 + $1))
bus_number=3
if [ "$1" -ge 0 ] && [ "$1" -le 2 ]; then
    bus_number=3;
elif [ "$1" -ge 3 ] && [ "$1" -le 5 ]; then
    bus_number=4;
fi

#MOCK
i2c_read() {
    # echo $1 $2 $3 $4
    if [ $3 = "0x9e" ]; then
        string="PSUSerialNumber"
    elif [ $3 = "0xab" ]; then
        string="PSUPartNumber"
    elif [ $3 = "0x9c" ]; then
        string="Delta Thailand"
    elif [ $3 = "0x9a" ]; then
        string="ECD16010092"
    elif [ $3 = "0xe2" ]; then
        string="01.05"
    fi
}
version() {
    i2c_read $bus_number $psu_number 0xe2 11
}
model() {
    i2c_read $bus_number $psu_number 0x9a 12
}
serial() {
    i2c_read $bus_number $psu_number 0x9e 11
}
part() {
    i2c_read $bus_number $psu_number 0xab 11
}
manufacturer() {
    i2c_read $bus_number $psu_number 0x9c 12
}
process_command() {
    command=$1
    if [ $command = "serial" ]; then
        serial
    elif [ $command = "part" ]; then
        part
    elif [ $command = "manufacturer" ]; then
        manufacturer
    elif [ $command = "model" ]; then
        model
    elif [ $command = "version" ]; then
        version
    fi
}
process_command $2
if [ $removenonacii -eq 1 ]
then
    echo $string | sed 's/[^a-z  A-Z 0-9]//g'
else
    echo $string
fi
