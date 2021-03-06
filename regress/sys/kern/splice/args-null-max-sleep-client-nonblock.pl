# test maximum data length with delay before empty client write with non-blocking relay

use strict;
use warnings;

our %args = (
    client => {
	func => sub { sleep 3; write_char(@_); },
	len => 0,
    },
    relay => {
	max => 113,
	nonblocking => 1,
    },
    len => 0,
    md5 => "d41d8cd98f00b204e9800998ecf8427e",
);

1;
