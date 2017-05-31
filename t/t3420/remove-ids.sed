s/^\(Created autostash: \)[0-9a-f]\{6,\}$/\1XXX/
s/^\(HEAD is now at \)[0-9a-f]\{6,\}\( .* commit\)$/\1XXX\2/
s/^[0-9a-f]\{6,\}\( .* commit\)$/XXX\1/
s/\(detached HEAD \)[0-9a-f]\{6,\}/\1XXX/
s/\(could not apply \)[0-9a-f]\{6,\}/\1XXX/g
s/
