
#!/bin/bash

MODULE_NAME="message_slot"
MODULE_PATH="/mnt/shared/EX3"

# Change to the module directory
cd $MODULE_PATH || { echo "Failed to change directory to $MODULE_PATH"; exit 1; }

# Check if the module is loaded and remove it if it is
if lsmod | grep "$MODULE_NAME" &> /dev/null ; then
    echo "Removing existing kernel module: $MODULE_NAME"
    sudo rmmod $MODULE_NAME
    if [ $? -ne 0 ]; then
        echo "Failed to remove the module $MODULE_NAME"
        exit 1
    fi
else
    echo "Module $MODULE_NAME is not currently loaded"
fi

# Clean the module build
echo "Cleaning the module build"
make clean

# Build the module
echo "Building the kernel module: $MODULE_NAME"
make

if [ $? -ne 0 ]; then
    echo "Failed to build the module $MODULE_NAME"
    exit 1
fi

# Insert the newly built module
echo "Loading the kernel module: $MODULE_NAME"
sudo insmod ./$MODULE_NAME.ko

if [ $? -ne 0 ]; then
    echo "Failed to load the module $MODULE_NAME"
    exit 1
fi

echo "Kernel module $MODULE_NAME reloaded successfully"
