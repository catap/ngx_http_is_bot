#!/usr/bin/perl

# (C) Kirill A. Korinskiy

# Tests for is_bot module.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib '../../tests/lib';
use Test::Nginx;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http/)->plan(2)
	->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

master_process off;
daemon         off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;

        is_bot_by    "$arg_bot";
        is_bot_data  %%TESTDIR%%/is_bot_data;

        if ($is_bot) {
            return 402;
        }
        return 404;
    }
}

EOF

my $d = $t->testdir();
$t->write_file('is_bot_data', 'BOT');

$t->run();

###############################################################################

my $r = http_get('/');

like($r, qr!^HTTP/1.1 404 Not Found!m, 'not bot');

$r = http_get('/?bot=BOT');
like($r, qr!^HTTP/1.1 402 Payment Required!m, 'is bot');

###############################################################################
