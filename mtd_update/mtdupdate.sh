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

if [ $# -ne 2 ] && [ $# -ne 3 ]; then
    exit 1
fi

setup_file="/usr/bin/setup_$2.sh"
cleanup_file="/usr/bin/cleanup_$2.sh"

target="$2"
if [ -n "$3" ]; then
    target="$3"
fi

#send fw update task a message that update has started
busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} Update.1.0.TransferringToComponent xyz.openbmc_project.Logging.Entry.Level.Informational 3 'REDFISH_MESSAGE_ARGS' " ,${target}" 'REDFISH_MESSAGE_ID' 'Update.1.0.TransferringToComponent' 'namespace' 'FWUpdate'

if [ -f "$setup_file" ] && [ -x "$setup_file" ]; then
#use paramter 1 to inform underlying script that it is executing during
#regular fw update process
    result=$("$setup_file" 1)
    exit_code=$?

    if [ $exit_code -ne 0 ]; then
        #send fw update task a message that update has failed
        busctl call xyz.openbmc_project.Logging /xyz/openbmc_project/logging xyz.openbmc_project.Logging.Create Create ssa{ss} org.open_power.Logging.Error.TestError1 xyz.openbmc_project.Logging.Entry.Level.Warning 3 'REDFISH_MESSAGE_ARGS' "${target}:${result}" 'REDFISH_MESSAGE_ID' 'ResourceEvent.1.0.ResourceErrorsDetected' 'namespace' 'FWUpdate'
        if [ -f "$cleanup_file" ] && [ -x "$cleanup_file" ]; then
            "$cleanup_file"
        fi
        exit $exit_code
    fi
fi

tail -c +4097 $1 > /run/initramfs/image-$2

#calculate md5sum of the update image to
#be used by the cleanup script for verification
#of the update process
expected_md5sum=$(md5sum "/run/initramfs/image-$2" | awk '{print $1}')
expected_size=$(stat -c%s "/run/initramfs/image-$2")
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

