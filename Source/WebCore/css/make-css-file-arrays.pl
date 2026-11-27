#!/usr/bin/env perl
#
#  Copyright (C) 2006 Apple Inc.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Library General Public
#  License as published by the Free Software Foundation; either
#  version 2 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Library General Public License for more details.
#
#  You should have received a copy of the GNU Library General Public License
#  along with this library; see the file COPYING.LIB.  If not, write to
#  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#  Boston, MA 02110-1301, USA.
#

# Usage: make-css-file-arrays.pl <header> <output> <input> ...

use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/../bindings/scripts";

use Getopt::Long;

my $defines;
my $preprocessor;
GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor);

my $header = $ARGV[0];
shift;

my $out = $ARGV[0];
shift;

open HEADER, ">", $header or die;
open OUT, ">", $out or die;

print HEADER "namespace WebCore {\n";
print OUT "namespace WebCore {\n";

# The sheets use only these directive forms:
#   #if defined(X) && X        #if !(defined(X) && X)
#   #if !defined(X) || !X      #if (defined(X) && X)
#   #else                      #endif [// comment]
# They are evaluated here instead of by an external preprocessor, which keeps
# the bytes the same on every port and does not depend on a host tool: on
# Windows the MSVC preprocessor printed nothing for these files, which turned
# every array below into a zero-length array and failed the build far from the
# cause.
my %macros = map {
    my ($name, $value) = split(/=/, $_, 2);
    ($name => defined $value ? $value : 1)
} grep { $_ } split(/\s+/, defined $defines ? $defines : '');

sub macroIsSet
{
    my ($macros, $name) = @_;
    return 0 unless exists $macros->{$name};
    return $macros->{$name} eq '0' ? 0 : 1;
}

# Splits on an operator that is not inside parentheses: '!(defined(A) && A)'
# is one operand, not two.
sub splitTopLevel
{
    my ($expression, $operator) = @_;
    my @parts;
    my $depth = 0;
    my $start = 0;
    for (my $i = 0; $i < length $expression; ++$i) {
        my $character = substr($expression, $i, 1);
        if ($character eq '(') {
            ++$depth;
        } elsif ($character eq ')') {
            --$depth;
        } elsif ($depth == 0 && substr($expression, $i, length $operator) eq $operator) {
            push @parts, substr($expression, $start, $i - $start);
            $i += length($operator) - 1;
            $start = $i + 1;
        }
    }
    push @parts, substr($expression, $start);
    return @parts;
}

# Whether the first parenthesis opens at the start and closes at the end.
sub isWrapped
{
    my ($expression) = @_;
    return 0 unless substr($expression, 0, 1) eq '(';
    my $depth = 0;
    for (my $i = 0; $i < length $expression; ++$i) {
        my $character = substr($expression, $i, 1);
        if ($character eq '(') {
            ++$depth;
        } elsif ($character eq ')') {
            --$depth;
            return $i == length($expression) - 1 if $depth == 0;
        }
    }
    return 0;
}

sub evaluateCondition
{
    my ($macros, $expression) = @_;
    $expression =~ s{//.*$}{};
    $expression =~ s/^\s+|\s+$//g;

    my @orParts = splitTopLevel($expression, '||');
    if (@orParts > 1) {
        for my $part (@orParts) {
            return 1 if evaluateCondition($macros, $part);
        }
        return 0;
    }

    my @andParts = splitTopLevel($expression, '&&');
    if (@andParts > 1) {
        for my $part (@andParts) {
            return 0 unless evaluateCondition($macros, $part);
        }
        return 1;
    }

    return evaluateCondition($macros, $1) ? 0 : 1 if $expression =~ /^!\s*(.+)$/;
    return evaluateCondition($macros, substr($expression, 1, length($expression) - 2)) if isWrapped($expression);
    return macroIsSet($macros, $1) if $expression =~ /^defined\s*\(\s*(\w+)\s*\)$/;
    return macroIsSet($macros, $1) if $expression =~ /^(\w+)$/;

    die "make-css-file-arrays.pl: cannot evaluate '$expression'\n";
}

sub preprocess
{
    my ($macros, $content) = @_;
    my @output;
    my @stack;
    my $emitting = 1;

    for my $line (split(/^/m, $content)) {
        if ($line =~ /^\s*#\s*if\s+(.*)$/) {
            my $condition = $emitting ? evaluateCondition($macros, $1) : 0;
            push @stack, [$emitting, $condition ? 1 : 0];
            $emitting = $emitting && $condition;
        } elsif ($line =~ /^\s*#\s*ifdef\s+(\w+)/) {
            my $condition = $emitting ? macroIsSet($macros, $1) : 0;
            push @stack, [$emitting, $condition ? 1 : 0];
            $emitting = $emitting && $condition;
        } elsif ($line =~ /^\s*#\s*ifndef\s+(\w+)/) {
            my $condition = $emitting ? !macroIsSet($macros, $1) : 0;
            push @stack, [$emitting, $condition ? 1 : 0];
            $emitting = $emitting && $condition;
        } elsif ($line =~ /^\s*#\s*elif\s+(.*)$/) {
            die "make-css-file-arrays.pl: #elif without #if\n" unless @stack;
            my $frame = $stack[-1];
            my $condition = $frame->[0] && !$frame->[1] ? evaluateCondition($macros, $1) : 0;
            $frame->[1] = 1 if $condition;
            $emitting = $frame->[0] && $condition;
        } elsif ($line =~ /^\s*#\s*else\b/) {
            die "make-css-file-arrays.pl: #else without #if\n" unless @stack;
            my $frame = $stack[-1];
            $emitting = $frame->[0] && !$frame->[1];
            $frame->[1] = 1;
        } elsif ($line =~ /^\s*#\s*endif\b/) {
            die "make-css-file-arrays.pl: #endif without #if\n" unless @stack;
            my $frame = pop @stack;
            $emitting = $frame->[0];
        } elsif ($line =~ /^\s*#/) {
            die "make-css-file-arrays.pl: unknown directive '$line'\n";
        } else {
            push @output, $line if $emitting;
        }
    }
    die "make-css-file-arrays.pl: unterminated #if\n" if @stack;
    return join('', @output);
}

for my $in (@ARGV) {
    $in =~ /(\w+)\.css$/ or $in =~ /(\w+)\.js$/ or die;
    my $name = $1;

    open my $sheet, "<", $in or die "make-css-file-arrays.pl: cannot read $in: $!\n";
    my $text = do { local $/; <$sheet> };
    close $sheet;

    # Comments first, so that a commented-out directive is not read as one.
    $text =~ s|/\*.*?\*/||gs;
    $text = preprocess(\%macros, $text);
    # Remove comments in a simple-minded way that will work fine for our files.
    # Could do this a fancier way if we were worried about arbitrary CSS source.
    $text =~ s|/\*.*?\*/||gs;

    # Crunch whitespace just to make it a little smaller.
    # Could do work to avoid doing this inside quote marks but our files don't have runs of spaces in quotes.
    # Could crunch further based on places where whitespace is optional.
    $text =~ s|\s+| |gs;
    $text =~ s|^ ||;
    $text =~ s| $||;

    # Write out a C array of the characters.
    my $length = length $text;
    if (!$length && $in =~ /(\w+)\.css$/) {
        # A sheet whose whole content is behind a disabled feature (the Symbian
        # port has ENABLE_FULLSCREEN_API off, which empties fullscreen.css). GCC
        # accepts a zero-length array, MSVC rejects it with C2466, so write one
        # NUL byte: the consumers parse an empty sheet either way.
        print HEADER "extern const char ${name}UserAgentStyleSheet[1];\n";
        print OUT "extern const char ${name}UserAgentStyleSheet[1] = { 0 };\n";
        next;
    }
    if ($in =~ /(\w+)\.css$/) {
        print HEADER "extern const char ${name}UserAgentStyleSheet[${length}];\n";
        print OUT "extern const char ${name}UserAgentStyleSheet[${length}] = {\n";
    } else {
        print HEADER "extern const char ${name}JavaScript[${length}];\n";
        print OUT "extern const char ${name}JavaScript[${length}] = {\n";
    }
    my $i = 0;
    while ($i < $length) {
        print OUT "    ";
        my $j = 0;
        while ($j < 16 && $i < $length) {
            print OUT ", " unless $j == 0;
            print OUT ord substr $text, $i, 1;
            ++$i;
            ++$j;
        }
        print OUT "," unless $i == $length;
        print OUT "\n";
    }
    print OUT "};\n";

}

print HEADER "}\n";
print OUT "}\n";
