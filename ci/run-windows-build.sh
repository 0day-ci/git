#!/usr/bin/env bash
#
# Script to trigger the a Git for Windows build and test run.
# Pass a token, the branch (only branches on https://github.com/git/git)
# are supported), and a commit hash.
#

[ $# -eq 3 ] || (echo "Unexpected number of parameters" && exit 1)

TOKEN=$1
BRANCH=$2
COMMIT=$3

gfwci () {
	curl \
		-H "Authentication: Bearer $TOKEN" \
		--silent --retry 5 \
		"https://git-for-windows-ci.azurewebsites.net/api/TestNow?$1" |
	sed "$(printf '1s/^\xef\xbb\xbf//')"  # Remove the Byte Order Mark
}

# Trigger build job
BUILD_ID=$(gfwci "action=trigger&branch=$BRANCH&commit=$COMMIT&skipTests=false")

# Check if the $BUILD_ID contains a number
case $BUILD_ID in
	''|*[!0-9]*) echo $BUILD_ID && exit 1
esac

echo "Visual Studio Team Services Build #${BUILD_ID}"

# Wait until build job finished
STATUS=
RESULT=
while true
do
	LAST_STATUS=$STATUS
	STATUS=$(gfwci "action=status&buildId=$BUILD_ID")
	[ "$STATUS" == "$LAST_STATUS" ] || printf "\nStatus: $STATUS "
	printf "."

	case $STATUS in
		inProgress|postponed|notStarted) sleep 10                      ;; # continue
		         "completed: succeeded") RESULT="success";        break;; # success
		                              *) echo "Unknown: $STATUS"; break;; # failure
	esac
done

# Print log
echo ""
echo ""
gfwci "action=log&buildId=$BUILD_ID" | cut -c 30-

# Set exit code for TravisCI
[ "$RESULT" == "success" ]
