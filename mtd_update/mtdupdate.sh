#!/bin/sh

#this script will call underlying update script to update given mtd partition
#the script will call setup and clean bash scripts if they exist
#in order to toggle potential gpio's that will expose the connections
#script will also use busctl to send messages to the Task so the
#user can be informed of fw update status.

run_command() {
    local cmd_output
    cmd_output="$($1 2>&1)"
    exit_code=$?
    echo "$cmd_output"
    return $exit_code
}

if [ $# -lt 2 ] || [ $# -gt 4 ]; then
    echo "Usage: $0 <input_file> <image_name> [target] [nostrip]"
    echo "  input_file:   The firmware image file to update"
    echo "  image_name:  Name used for setup/cleanup scripts (setup_<image_name>.sh)"
    echo "  target:       Optional target name for logging (defaults to image_name)"
    echo "  nostrip:      Optional flag to copy file without stripping first 4096 bytes"
    exit 1
fi

# Check for nostrip option
nostrip=0
input_file="$1"
image_name="$2"
target="$2"

# Check all arguments for nostrip flag
for arg in "$@"; do
    if [ "$arg" = "nostrip" ]; then
        nostrip=1
    fi
done

# Find target name (third argument if it exists and is not nostrip)
if [ $# -ge 3 ]; then
    if [ "$3" != "nostrip" ]; then
        target="$3"
    fi
fi

setup_file="/usr/bin/setup_${image_name}.sh"
cleanup_file="/usr/bin/cleanup_${image_name}.sh"

if [ -f "$setup_file" ] && [ -x "$setup_file" ]; then
#use paramter 1 to inform underlying script that it is executing during
#regular fw update process
    result=$("$setup_file" 1)
    exit_code=$?

    #if 255 is returned it means update should not be done
    if [ $exit_code -eq 255 ]; then
        exit 0
    fi

    if [ $exit_code -ne 0 ]; then
        #send fw update task a message that update has failed
        busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} org.open_power.Logging.Error.TestError1 xyz.openbmc_project.Logging.Entry.Level.Warning 3 'REDFISH_MESSAGE_ARGS' "${target}:${result}" 'REDFISH_MESSAGE_ID' 'ResourceEvent.1.0.ResourceErrorsDetected' 'namespace' 'FWUpdate'
        if [ -f "$cleanup_file" ] && [ -x "$cleanup_file" ]; then
            "$cleanup_file"
        fi
        exit $exit_code
    fi
fi
#send fw update task a message that update has started
busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} Update.1.0.TransferringToComponent xyz.openbmc_project.Logging.Entry.Level.Informational 3 'REDFISH_MESSAGE_ARGS' " ,${target}" 'REDFISH_MESSAGE_ID' 'Update.1.0.TransferringToComponent' 'namespace' 'FWUpdate'

if [ $nostrip -eq 1 ]; then
    cp "$input_file" "/run/initramfs/image-${image_name}"
else
    tail -c +4097 "$input_file" > "/run/initramfs/image-${image_name}"
fi

#calculate md5sum of the update image to
#be used by the cleanup script for verification
#of the update process
expected_md5sum=$(md5sum "/run/initramfs/image-${image_name}" | awk '{print $1}')
expected_size=$(stat -c%s "/run/initramfs/image-${image_name}")
cd /run/initramfs
result=$(run_command "./update")
exit_code=$?

if [ $exit_code -ne 0 ]; then
    #send fw update task a message that update has failed
    busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} org.open_power.Logging.Error.TestError1 xyz.openbmc_project.Logging.Entry.Level.Warning 3 'REDFISH_MESSAGE_ARGS' "${target}:${result}" 'REDFISH_MESSAGE_ID' 'ResourceEvent.1.0.ResourceErrorsDetected' 'namespace' 'FWUpdate'
    if [ -f "$cleanup_file" ] && [ -x "$cleanup_file" ]; then
        "$cleanup_file"
    fi
    exit $exit_code
fi

if [ -f "$cleanup_file" ] && [ -x "$cleanup_file" ]; then
#provide 1 as a parameter that the underlying script is running
#under the regular fw update process and copy of the binary
#installed for the fw update verification if needed
    result=$("$cleanup_file" 1 "$expected_md5sum" "$expected_size")
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        #send fw update task a message that update has failed
        busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} org.open_power.Logging.Error.TestError1 xyz.openbmc_project.Logging.Entry.Level.Warning 3 'REDFISH_MESSAGE_ARGS' "${target}:${result}" 'REDFISH_MESSAGE_ID' 'ResourceEvent.1.0.ResourceErrorsDetected' 'namespace' 'FWUpdate'
        exit $exit_code
    fi

fi

#send fw update task a message that update was success
busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} Update.1.0.UpdateSuccessful xyz.openbmc_project.Logging.Entry.Level.Informational 3 'REDFISH_MESSAGE_ARGS' "${target}" 'REDFISH_MESSAGE_ID' 'Update.1.0.UpdateSuccessful' 'namespace' 'FWUpdate'
exit $exit_code

