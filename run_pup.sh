#!/bin/bash

# Run pup, and install pup if it is not present.

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )


python $DIR/scripts/pup/pup $* "$DIR/scripts/pup/pup.conf"

