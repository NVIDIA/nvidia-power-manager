#!/bin/sh

string=''

string=''
i2c_read() {
    if [ "$3" == "0x9e" ]; then 
        string="CPLDSerialNumber"
    elif [ "$3" == "0xab" ]; then
        string="CPLDPartNumber"
    elif [ "$3" == "0x99" ]; then
        string="NVIDIAManufacture"
    elif [ "$3" == "0x9a" ]; then
        string="CPLDModel"
    elif [ "$3" == "0x2d" ]; then
        string="CPLDVersion"
    fi
}


if [ $# -eq 0 ]; then
    echo 'No device is given' >&2
    exit 1
fi

input=$(echo $1 | tr "-" " ")
arr=(${input// / });
i2c_read ${arr[1]} ${arr[2]} ${arr[3]} ${arr[4]}
echo $string | sed 's/[^a-z  A-Z 0-9]//g'
