#!perl

$rdir=shift || &printargs;
$map=shift || &printargs;
$mod=shift || &printargs;
$startdate=shift || &printargs;
$enddate=shift || &printargs;
$dateinc=shift || &printargs;

die "bad date format $startdate" unless $startdate=~s@^(\d\d\d\d)/(\d\d)/(\d\d)$@\1\2\3@;

die "bad date format $enddate" unless $enddate=~s@^(\d\d\d\d)/(\d\d)/(\d\d)$@\1\2\3@;

$jday=MJD($startdate);
$jday1=MJD($enddate);

for($day=$jday;$day<=$jday1;$day+=$dateinc)
  {
	($y, $m, $d)=DJM($day);
	$p4cmd="p4 sync $rdir\\...\@$y/$m/$d:01:00:00 >nul 2>&1";
	$hl2cmd="$rdir\\hl2 -game $mod -sw +map $map -makedevshots -dev -width 1024 -height 768";

	print "Taking shots for $m/$d/$y\n";
	print "$p4cmd\n";
	print `$p4cmd`;
	print "hl2cmd\n";
	print `$hl2cmd`;
  }

sub printargs
  {
	print STDERR "format is SHOTMAKER.PL rootdir mapname mod startdate enddate dateincrement\n";
	print STDERR "ex:\nSHOTMAKER u:\\dev\\valvegames\\main\\game ep1_c17_01 episodic 2005/10/01 2005/10/05 7\n";
	die;

  }















# Toby Thurston --- 12 May 2003

use strict;
use Carp;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS @mon @dom);

require Exporter;
@ISA = qw(Exporter);

$VERSION = '0.03';

=head1 NAME

Cal::Date - a simple set of calendar functions for Perl

(yes, yes, I know about L<Date::Calc> and L<Date::Manip> but mine is
simpler, and nicer :-).

=head1 SYNOPSIS

  use Cal::Date qw(DJM MJD today);
  $date = $ARGV[0] || today();
  print "$date --> " . MJD($date) . "\n";
  print "Day after -->" . DJM(MJD($date)+1) . "\n";

=head1 DESCRIPTION

A simple compact interface to some simple calendar routines.
Implemented purely in Perl, no need for external C code etc.

=head1 FUNCTIONS

No functions are exported by default.

=cut

@EXPORT = qw();

=pod

The following functions can be exported from the C<Cal::Date> module:

    MJD DJM
    Easter old_style_Easter orthodox_Easter
    ISO_week ISO_day ISO_week_and_day
    day_of_year days_to_go
    days_in_month
    UK_tax_week UK_tax_month
    working_days
    today now
    J2G
    v_date r_date
    adjust_to_local_time adjust_to_UTC
    is_a_date

=cut

@EXPORT_OK = qw(
    MJD DJM
    Easter old_style_Easter orthodox_Easter
    ISO_week ISO_day ISO_week_and_day
    day_of_year days_to_go
    days_in_month
    UK_tax_week UK_tax_month
    working_days
    today now
    J2G
    v_date r_date
    adjust_to_local_time adjust_to_UTC
    is_a_date
);

=pod

You can import all of them at once with
  C<use Cal::Date ':all';>

=cut

%EXPORT_TAGS = (all => [@EXPORT_OK]);

=over 4

=item MJD(yyyymmdd) or MJD(y,m,d)

MJD returns the `modified julian day' number for a date.
This is suitably small integer that you can use as the basis of many
date calculations.  You can call C<MJD()> with a single 8 digit string
representing a date in compact ISO form, C<yyyymmdd>, or with three integers
representing year, month and day of the month.

Unlike the values returned from the C<gmtime()> etc. functions,
year is the full AD year and month 1 is January.   Other than checking
that the arguments are whole numbers, the internal function C<_getYMD>
does no range checking.  This is a feature rather than a bug.  It means
you can use 0 as a month number to refer to December in the previous year,
and 13 to refer to January in the next year.  For example,
assuming C<$month == 12>, the following are equivalent:

   MJD(19991301);
   MJD(1999, $month+1, 1);
   MJD(20000101);
   MJD(2000, 1, 1);

You can do the same trick  with the day numbers too; this provides a handy
way to refer to the last day of the previous month.  Thus C<MJD(20000100)>
refers to 31 December 1999 (but note that C<MJD(20000000)> refers to
30 November 1999).  This works with leap years too (of course) so
C<MJD($y,3,0)> refers to the last day of February for any value of C<$y>.

=cut

sub MJD {          # returns mjd from yyyymmdd or y,m,d
    use integer;
    my ($y, $m, $d) = &_getYMD;
    # allow month to be enormous
    while ( $m > 12) { $m -= 12; $y++ }
    # adjust the month/year to make it a date after 1 March
    if ($m < 3) { $m += 12; $y-- }
    # work out days upto and including the day before the previous 1 March
    # year * 365 + leap days - 306
    # we are using the (possibly proleptic) Gregorian calendar
    my $mjd = $y*365 + $y/4 - $y/100 + $y/400 - 306;
    # add days since previous 1 March (incl)
    $mjd += ($m+1)*306/10 - 122 + $d;
    # adjust so 0 == 18 Nov 1858 == JD 2,400,000.5
    $mjd -= 678576;
    return $mjd;
}

=item DJM(mjd)

This function is the inverse of the C<MJD()> function, hence the rather
cute name.  It takes any number, interprets it as an MJD number and returns
the corresponding date in the ISO compact form of YYYYMMDD.  This form has
the advantage of being easily sorted and compared.

C<DJM()> is often used in combination with MJD.  For example to `correct'
a date use C<DJM(MJD(yyyymmdd))>.  If your input date was 20000300, this will
return 20000229.  This idiom can also be used to check that an input date is
valid.  Like this:

    if ($date ne DJM(MJD($date)) ) {
        print "$date is not a valid YYYYMMDD date\n";
    }

When you pass a real number to C<DJM()> the fractional part is interpreted
as a fraction of a day, and the date and time are returned in C<YYYYMMDD HH:MM>
form.   Like this:

    print DJM(51455.7356) . "\n"; # prints 19991004 17:39

If you call C<DJM()> in a list context then the parts of the date/time
are returned as elements of a list, like this:

    ($y, $m, $d, $hr, $min) = DJM(51455.7356);
    ($y, $m, $d) = DJM(51500);


=cut

sub DJM {                                         # returns yyyymmdd from mjd
    return unless defined wantarray;                # don't bother doing more
    # the supplied MJD may be integer (hour=midnight) or real
    # the fractional part repesents the time of day
    my $mjd = shift;
    # convert to full Julian number
    my $jd  = $mjd + 2400000.5;

    # jd0 is the Julian number for noon on the day in question
    # for example   mjd      jd     jd0   === mjd0
    #               3.0  ...3.5  ...4.0   === 3.5
    #               3.3  ...3.8  ...4.0   === 3.5
    #               3.7  ...4.2  ...4.0   === 3.5
    #               3.9  ...4.4  ...4.0   === 3.5
    #               4.0  ...4.5  ...5.0   === 4.5
    my $jd0 = int($jd+0.5);

    # next we convert to Julian dates to make the rest of the maths easier.
    # JD1867217 = 1 Mar 400, so $b is the number of complete Gregorian
    # centuries since then.  The constant 36524.25 is the number of days
    # in a Gregorian century.  The 0.25 on the other constant ensures that
    # $b correctly rounds down on the last day of the 400 year cycle.
    # For example $b == 15.9999... on 2000 Feb 29 not 16.00000.
    my $b = int(($jd0-1867216.25)/36524.25);

    # b-int(b/4) is the number of Julian leap days that are not counted in
    # the Gregorian calendar, and 1402 is the number of days from 1 Jan 4713BC
    # back to 1 Mar 4716BC.  $c represents the date in the Julian calendar
    # corrected back to the start of a leap year cycle.
    my $c = $jd0+($b-int($b/4))+1402;

    # d is the whole number of Julian years from 1 Mar 4716BC to the date
    # we are trying to find.
    my $d = int(($c+0.9)/365.25);

    # e is the number of days from 1 Mar 4716BC to 1 Mar this year
    # using the Julian calendar
    my $e = 365*$d+int($d/4);

    # c-e is now the remaining days in this year from 1 Mar to our date
    # and we need to work out the magic number f such that f-1 == month
    my $f = int(($c-$e+123)/30.6001);

    # int(f*30.6001) is the day of the start of the month
    # so the day of the month is the difference between that and c-e+123
    my $day = $c-$e+123-int(30.6001*$f);

    # month is now f-1, except that Jan and Feb are f-13
    # ie f 4 5 6 7 8 9 10 11 12 13 14 15
    #    m 3 4 5 6 7 8  9 10 11 12  1  2
    my $month = ($f-2)%12+1;

    # year is d - 4716 (adjusted for Jan and Feb again)
    my $year = $d - 4716 + ($month<3);

    # finally work out the hour (if any)
    my $hour = 24 * ($jd+0.5-$jd0);
    if ( $hour == 0) {
        if (wantarray) {
            return ($year, $month, $day)
        }
        else {
            return sprintf "%d%02d%02d", ($year, $month, $day)
        }
    }
    else {
        $hour = int($hour*60+0.5)/60;               # round to nearest minute
        my $min = int(0.5+60 * ($hour - int($hour)));
        $hour = int($hour);
        if (wantarray) {
            return $year, $month, $day, $hour, $min
        }
        else {
            return sprintf "%d%02d%02d %02d:%02d", $year, $month, $day, $hour, $min
        }
    }
}


=item today() or today(delta)

This function returns today's date in YYYYMMDD form, saving you
all that tedious mucking about with lists and C<undef>s.

It uses C<localtime()> so you get the date adjusted for local time
zone, depending on the time of day this may or may not be the same
as the date at Greenwich.  Use C<adjust_to_UTC> to get the UTC date if
that's what you want.

You can supply a number of days as an optional parameter.  This number (which
may be negative) will be added to the current date.  The number should be a
either a whole number of days or a week specification in a form that will
match C</^[+-]?\d+[wW]\d?$/>.  For example: C<1w> means one week, C<-2w3>
means -17 days.

=cut

sub today {                                       # return YYYYMMDD for today
    return unless defined wantarray;
    my $delta = &_get_delta;
    return DJM(MJD()+$delta);
}

sub _get_delta {
    my $delta = shift || 0;
    if ($delta =~ /^([+-])?(\d+)[wW](\d)?$/) {
        local $^W=0; # disable warnings for unitialized $1 or $3
        $delta = $1.($2*7+$3)
    }
    if ( $delta !~ /^([+-]?\d+)$/ ) {
        croak "Bad value for day shift: $delta\n";
    }
    return $delta;
}


sub now {                                       # return hh:mm for now
    return unless defined wantarray;
    my ($s, $m, $h) = localtime();
    return wantarray ? ($h,$m,$s) : sprintf("%02d:%02d:%02d", $h, $m, $s);
}


=item Easter(year,[delta])

This function takes a year number and returns the date of Easter Sunday
in YYYYMMDD form for that year.  See below about valid years.  The date
is supposed to be the first Sunday after the calendar full moon which
occurs on or after 21 March.  The name Easter comes from the Saxon
goddess of the dawn, Eostre, whose festival was celebrated at the vernal
equinox.

You can supply a number of days as an optional parameter.  This number
(which may be negative) will be added to the resulting date.  This is
handy for working out dates that depend on Easter.  For example:

     $y = 2000;
     $s = Easter($y,-47); # Shrove Tuesday (Pancake Day)
     $m = Easter($y,-21); # Mothers day in the UK
     $a = Easter($y,+39); # Ascension day

The format of the number should be as described above under L<today()>.

The algorithm used was adapted from D. E. Knuth I<Fundamental
Algorithms>, as Knuth notes it is derived from older sources, and is
only valid after 1582 when the Gregorian calendar was first used in
Europe (but not in Britain).  For years before this use the
L<old_style_Easter()> routine below, which returns Julian dates such as
were in use then.  I have only validated this routine back to 1066, the
earliest I could find a list in my reference books at home, but it
should be valid further back.  I do not know when Easter was first
celebrated as Easter.

=cut

sub Easter {
    return unless defined wantarray;    # don't bother doing more
    use integer;
    my $y = shift;
    my $delta = &_get_delta;
    my $golden = $y%19 + 1;
    my $century = $y/100 + 1;
    my $x = 3*$century/4 - 12;
    my $q = 5*$y/4 - $x - 10;

    my $epact = (11*$golden + 15 + (8*$century + 5)/25 - $x) % 30;
     ++$epact if ($epact == 25 && $golden > 11) || $epact == 24;

    my $d = 44 - $epact;
       $d += 30 if $d < 21;
       $d = $d + 7 - (($q+$d)%7);

    return DJM(MJD($y,3,$d)+$delta);
}

=item old_style_Easter(year,[delta])

This function is mainly of historical interest.  Before the switch to
Gregorian dates that happened in 1582 in certain parts of Roman Catholic
Europe, the Julian calendar was used.  This routine gives you the date
of Easter in the Julian calendar.  Because of the way Easter is derived,
this is not a constant number of days apart from the date in Gregorian.
Typically it can be either 4 or 5 weeks or just a few days.

In British historical records between 1582 and 1752 (when Britain
switched) the Julian dates are referred to as `old style' and the
Gregorian dates as `new style'.  Hence my name for this function.  This
algorithm is based on details found on the web which referred to the
algorithm of Oudin (1940), quoted in I<Explanatory Supplement to the
Astronomical Almanac>, P. Kenneth Seidelmann, editor.

You can add an optional day shift number as above in L<Easter()>.

=cut

sub old_style_Easter {
    return unless defined wantarray;    # don't bother doing more
    use integer;
    my $y = shift;
    my $delta = &_get_delta;
    my $g = $y % 19;
    my $i = (19*$g + 15) % 30;
    my $j = ($y + $y/4 + $i) % 7;
    my $l = $i - $j;
    my $m = 3 + ($l + 40)/44;
    my $d = $l + 28 - 31*($m/4);
    return DJM(MJD($y,$m,$d)+$delta);
}


=item orthodox_Easter(year,[delta])

The various Orthodox parts of the Christian church (principally in Greece, the
Balkans and other parts of eastern Europe and Russia) still use the Julian calendar
(the `old style') to work out the date of Easter, but they express the result
in new style, Gregorian dates.  This routine may be handy if you belong to such
a church or if you are planning a spring holiday in Greece, where Easter is always
a special time.

This is essentially just old_style_Easter corrected to Gregorian dates with the L<J2G()> function.

=cut

sub orthodox_Easter {
    my ($y,$m,$d) = &old_style_Easter;
    return DJM(MJD($y,$m,$d)+J2G($y,$m,$d));
}


=item ISO_week(yyyymmdd) or ISO_week(y,m,d)

This function returns the week number according to the ISO standard.
This states that weeks begin on a Monday (day 1), and that the first
week of a year is the one with 4 Jan in it.  The function returns the
date in the ISO week form: yyyy-Wnn.  The year is included as it may
differ from the year of the date in yyyymmdd form.  For example
C<ISO_week(20000101)> returns C<1999-W52>.

The ISO day number for a given date is given by C<ISO_day()>.  See below.

=cut

sub ISO_week {
    return unless defined wantarray;    # don't bother doing more
    use integer;
    my ($y, $m, $d) = &_getYMD;
    my $jan1 = MJD($y,1,1);
    my $week = (MJD($y,$m,$d) - $jan1 + 1 + ($jan1+5)%7 + 3) / 7;
    if ( $week == 0 ) {
        # week belongs to last year
        $y--;
        # work out if its W52 or W53
        $jan1 = MJD($y,1,1);
        $week = (MJD($y,12,31) - $jan1 + 1 + ($jan1+5)%7 + 3) / 7;
    }
    elsif ( $week == 53 ) {
        # week might belong to next year
        # if 31 Dec is Weds or earlier
        if (ISO_day(MJD($y,12,31)) < 4) {
            $y++;
            $week = 1;
        }
    }
    return wantarray ? ($y, $week) : sprintf "%d-W%02d", $y, $week;
}


=item ISO_day(mjd)

This function returns the ISO day number for a given MJD value.
According to ISO, Monday is day 1 and Sunday day 7 in the week.
To find today's ISO day number do:

    print ISO_day(MJD(today()));

I occasionally find that I call this with a date by mistake for an MJD
number, so as a convenience if the MJD number is over 10,000,000 we will
interpret it as a date.  This means that ISO_day won't work for dates
after 29237-12-12, which we can probably live with, but that
c<ISO_day(20010117)> gives a less astonishing result.

=cut

sub ISO_day {
    my $mjd = shift;
    if ($mjd > 10_000_000) {
        $mjd = MJD($mjd);
    }
    if ($mjd > -3) {
        return ($mjd+2)%7+1;
    }
    else {
        return abs(9+$mjd%7)%7+1;
    }
}

=item ISO_week_and_day(yyyymmdd) or ISO_week_and_day(y,m,d)

Converts a given date to ISO Week.Day form, sometimes known as business
date form. For example 19991215 maps to 1999-W51-6

=cut

sub ISO_week_and_day {
    return unless defined wantarray;                # don't bother doing more
    return wantarray ? (&ISO_week, ISO_day(&MJD)) : &ISO_week . '-' . ISO_day(&MJD)
}





=item day_of_year(yyyymmdd) or day_of_year(y,m,d)

This function returns the day number of the current year, where Jan 1 = 1,
Feb 1 = 32 etc.  It is implemented simply as

   MJD($y,$m,$d) - MJD($y-1,12,31)

=cut

sub day_of_year {
    my ($y, $m, $d) = &_getYMD;
    return MJD($y,$m,$d)-MJD($y,1,0);
}

=item days_to_go(yyyymmdd) or days_to_go(y,m,d)

This function returns the days to the end of the year, where Dec 31 = 0,
Dec 30 = 1, etc.  Again it is simply implemented as

    MJD($y,12,31)-MJD($y,$m,$d);

=cut

sub days_to_go {
    my ($y, $m, $d) = &_getYMD;
    return MJD($y,12,31)-MJD($y,$m,$d);
}

=item days_in_month(y,m)

This function returns the days in the current month.  It is implemented
like this:

    MJD($y,$m+1,1)-MJD($y,$m,1);

Note that this works even in December (when C<$m==12>)
because C<MJD()> interprets 13 to mean January next year.

You may find it easier to use MJD directly for this function, and save
an import.

=cut

sub days_in_month {
    my ($y, $m) = @_;
    return MJD($y,$m+1,1)-MJD($y,$m,1);
}

=item UK_tax_week(yyyymmdd) or UK_tax_week(y,m,d)

This function is specific to UK Income Tax or `Pay As You Earn' rules.
It returns a string indicating the week in the tax year corresponding to a
given date.  The UK tax year starts on April 5 each year.  Example:

    print UK_tax_week(19991225);  # Prints: PAYE Week 38

=cut

sub UK_tax_week {
    my ($y, $m, $d) = &_getYMD;
    my $april6 = MJD($y,4,6);
    my $today  = MJD($y,$m,$d);
    if ($april6 > $today ) { $april6 = MJD($y-1,4,6) }
    use integer;
    return sprintf "%d", ($today-$april6)/7+1;
}

=item UK_tax_month()

This function is also specific to UK Income Tax or `Pay As You Earn' rules.
It returns a string indicating the month in the tax year corresponding to a
given date.  The UK tax year starts on April 5 each year.  Example:

    print UK_tax_month(19991225);  # Prints: PAYE Month 9

=cut

sub UK_tax_month {
    my ($y, $m, $d) = &_getYMD;
    return sprintf "%d", ($m+8-($d<6))%12+1;
}




=item working_days(y,m,d,period) or working_days(y,m,d,y2,m2,d2)

This function returns the number of working days in a given period including
start day.  Call it with a date and a number of days or with two dates.  The
number of days returned is simply the number of non-weekend days, no account
is taken of holidays etc.  More sophisticated functions can be found in the
C<Date::Manip> package.  The two dates can be given in either order.  Should
they be the same, then 1 or 0 may be returned depending on whether the day in
question was a working day or not.

=cut

sub working_days {
    my ($start,$end,$m,$count);
    $start = MJD($_[0],$_[1],$_[2]);

    if    (@_ == 4) { $end   = $start + $_[3] - 1; }
    elsif (@_ == 6) { $end   = MJD($_[3],$_[4],$_[5]); }
    else            { croak "Bad call to working days: $!\n" }

    if ($start > $end ) { ($start,$end) = ($end,$start)}
    if ($end-$start > 10000 ) { return 'Lots' }

    $count = 0;
    for $m ($start..$end) {
        ++$count if ISO_day($m) < 6
    }

    return $count;
}

=item v_date(year,datespec[,delta])

v_date returns a date as a real MJD (or (y,m,d,h,min,s) in list context)
optionally shifted by delta days, based on the specification in datespec
and the given year.
The format of the delta number should be as described above under L<today()>.

This specification can be one of the standard variable date forms used in
setting a Posix TZ environment variable, extended as noted here.

The main form is Mmm.w.d where `mm' is the month (1-12) number, `w' is the
week of the month (1-5 or L) note that 5 and L are equivalent and refer to
the last week of the months (either the fourth or fifth depending on the
length of the month), and `d' is the day of the week (0-7) where 1 = Monday
and 7 (or 0) = Sunday.

The use of L and 7 above are extensions to the Posix rules.  Further you can
extend the meaning of `d' to allow you to specify for example the last working
day in a month.  You do this by adding to the d number, eg:

    M10.L.12345  means the last working day of October, while
    M1.1.67      means the first weekend day in January.

Other forms are...

- Jddd which refers to the day of the year, regardless of leap days (ie 1
  March is always day J60 etc).

- ddd which refers to the day of the year counting leap days, (ie day 60 is
  Feb 29 in leap years or Mar 1 in non-leap years.

- Dmm.d.w which is exactly the same as the M form, but with the w and d
  fields reversed.

Any of the specs may be followed by "/hh[:mm[:ss]]" to indicate a particular
time.

v_date returns undef if called with an invalid spec.

=cut


sub v_date {
    return unless defined wantarray;
    my $y = shift;
    my $spec = shift;
    my $delta = &_get_delta;
    my ($m,$w,$d,$mjd,$time,$dshift);

    # remove any time from spec
    if ( $spec =~ /(.*)\/(\d+)(:(\d+)(:(\d+))?)?/ ) {
        $time = $2;
        if ( defined($4) ) {
            $time += $4/60;
            if ( defined($6) ) {
                $time += $6/3600;
            }
        }
        $spec = $1;
    }
    else { $time = 0 }

    # change D.... to M....
    if (($m,$d,$w) = $spec =~ /^D([0-1]?\d).([0-7]+).([1-5L])$/ ) {
        $spec = "M$m.$w.$d";
    }
    # Mmm.w.d
    if (($m,$w,$d) = $spec =~ /^M([0-1]?\d).([1-5L]).([0-7]+)$/ ) {
        if ($w =~ /[1-4]/ ) {
            $mjd = MJD($y,$m,1) + 7*($w-1);
            $dshift = 7;
            for my $n ( split(/ */,$d)) {
               $n = $n - ISO_day($mjd);
               if ($n<0) { $n += 7 }
               if ($n<$dshift) { $dshift = $n }
            }
        }
        else {                                                       # 5 or L
            $mjd = MJD($y,$m+1,0);
            $dshift = 7;
            for my $n ( split(/ */,$d) ) {
                $n = $n - ISO_day($mjd)%7;
                if ($n>0) { $n -= 7 }
                if (abs($n)<abs($dshift)) { $dshift = $n }
            }
        }
        $mjd = $mjd+$dshift+$delta;
    }
    # Jnnn ....
    elsif (($d) = $spec =~ /^J(\d+)$/ ) {
        if ($d>59) { $mjd = MJD($y,3,1)+$d-60+$delta }
        else       { $mjd = MJD($y,1,0)+$d+$delta    }
    }
    # nnn ...
    elsif (($d) = $spec =~ /^(\d+)$/ ) {
                     $mjd = MJD($y,1,0)+$d+$delta
    }
    else {
        croak "Malformed spec for v_date: $spec\n";
    }
    $mjd += $time/24;
    return wantarray ? DJM($mjd) : $mjd;
}

=item r_date(dow[,every[,start[,end]]])

This routine generates a list of MJD integers corresponding to a set of
repeating dates defined by the argument list.  The set may be empty in which
case an empty list is returned.  In the scalar context you get the number of
dates in the list.  The list is returned sorted in ascending numerical order.

dow: should match C</\d/ & /^1?2?3?4?5?6?7?$/>, that is at least one and
at most seven digits between 1 and 7 with no repetitions.  So "1" means
Mondays, "6" means Saturdays, "14" means Mondays and Thursdays and so on.

every: 1 means every dow, 2 means every other dow, 3 means every third dow, etc.
Every defaults to 1.

start: is a date in yyyymmdd form.  The first date in the returned list
will be on or after this date.  Start defaults to Jan 1st in the current year.

end: is another date in yyyymmdd form.  The last date in the returned list
will be on or before this date.  End defaults to Dec 31st in the current year.

Some examples:

   r_date(1)    returns a list of every Monday in the current year
   r_date(2,2,20030101,20030700)
                returns every other Tuesday in the first half of 2003
   r_date(15,1,20030501,20030531)
                every Monday and Friday in June 2003

=cut

sub r_date {
    return unless defined wantarray;
    my (undef,undef,undef,undef,undef,$y) = localtime;
    my $days  = shift;
    my $every = shift;
    my $start = shift;
    my $end   = shift;
    return undef unless defined $days  && $days  =~ /\d+/ && $days =~ /^1?2?3?4?5?6?7?$/;
    $every = 1   unless defined $every && $every =~ /^\d+$/ && $every<100;
    if ( defined $start && $start=~/^\d{8}$/ ) { $start = MJD($start) }
    else                                       { $start = MJD($y,1,1) }
    if ( defined $end   && $end  =~/^\d{8}$/ ) { $end = MJD($end)     }
    else                                       { $end = MJD($y,12,31) }

    my @list = ();

    for my $dow ( split / */, $days) {
        my $day_shift = $dow - ISO_day($start);
        $day_shift += 7 if $day_shift < 0;
        my $first_date = $start + $day_shift;
        for (my $i=0; $first_date+$i<$end; $i+=7*$every) {
            push @list, $first_date+$i;
        }
    }
    return sort @list;

}

=item adjust_to_local_time(mjd,tzoffset,tzrule1,tzrule2[,DST_delta])

This routine takes a real MJD number --- representing a UTC date and time ---
and adjusts it for time zone making proper allowance for summer time or
`daylight saving time' (DST).  The second argument is the normal difference
between UTC and local time (ie New York = +5) in hours.

The third and fourth arguments are two rules that define when DST should
start when it should stop.  If the rules are empty or undefined then the
routine returns the MJD adjusted to local time with no allowance for summer
time.  The rules are rules in the format understood by C<v_date()>.

The fifth argument represents the number of hours that the clocks go forward
when DST starts.  If this is omitted it will default to 1. This default was
not always correct historically but as far as I have been able to verify it
is currently, so you can nearly always omit the fifth argument.


=cut


sub adjust_to_local_time {
    my $mjd = shift;
    my $tz = shift || $Cal::Astro::tz;
    my $r1 = shift || $Cal::Astro::r1;
    my $r2 = shift || $Cal::Astro::r2;
    my $dst_delta = shift || 1;

    # stop here if no date given
    return '' unless defined($mjd);
    return '' if $mjd eq '';

    # stop here if no TZ given
    return $mjd unless defined($tz);

    # adjust for time zone
    $mjd = $mjd-$tz/24;

    # stop here if no summer time rules
    return $mjd unless defined($r1) && defined($r2);

    # make rules into dates for the current year
    my ($year) = DJM($mjd);
    my $d1 = v_date($year,$r1);
    my $d2 = v_date($year,$r2);

    # are we in DST at the start of the year?
    # (ie does r1 say October rather than March/April)
    my $jan_state = ($d1 > $d2);

    # swap the dates so that d1 < d2
    ($d1,$d2) = ($d2,$d1) if $jan_state;

    # if the date is in the summer set the opposite of
    # the state at the start of the year & adjust if needed
    if ($d1 <= $mjd && $mjd < $d2 ) {
        return $mjd + $dst_delta/24 * !$jan_state;
    }

    # otherwise return the state at the start of the year
    return $mjd + $dst_delta/24 * $jan_state;
}


=item adjust_to_UTC(mjd,tzoffset,tzrule1,tzrule2[,DST_delta])

This routine takes a real MJD number --- representing a local date and time ---
and adjusts it back to UTC allowing for local time zone and
summer time rules.

The arguments are all exactly the same as those for C<adjust_to_local_time()>.

=cut

sub adjust_to_UTC {
    my $mjd = shift;
    my $tz = shift || $Cal::Astro::tz;
    my $r1 = shift || $Cal::Astro::r1;
    my $r2 = shift || $Cal::Astro::r2;
    my $dst_delta = shift || 1;
    return adjust_to_local_time($mjd,-$tz,$r1,$r2,-$dst_delta);
}


sub is_a_date {
    my $date = shift;
    $date =~ s/[^0-9]//g;
    return 0 unless $date =~ /\d{8}/;
    return $date eq DJM(MJD($date));
}


sub _getYMD {
    my ($y, $m, $d);
    if ( @_ == 0 ) {

        (undef, undef, undef, $d, $m, $y) = localtime();
        $y += 1900;
        $m ++;

    } elsif ( @_ == 1 && !defined $_[0] ) {

        my ($package, $filename, $line) = caller;
        croak "\nCal::Date routine called with undefined value by $package \nLook at $filename, line $line\n";

    } elsif ( @_ == 1 && $_[0] =~ /^\d+$/ && $_[0] > 100000 ) {
        $y = substr($_[0],0,-4);
        $m = substr($_[0],-4,2);
        $d = substr($_[0],-2);
    } elsif ( @_ == 1 && $_[0] =~ /^\d+$/ ) {
        # probably an MJD as it is so small
        ($y, $m, $d) = DJM($_[0]);
    } elsif (@_ == 1 && $_[0] =~ /^(\d\d\d\d)-(\d\d)-(\d\d)$/ ) {
        ($y, $m, $d) = ($1, $2, $3);
    } elsif ( @_ == 3
            && $_[0] =~ /^\d+$/
            && $_[1] =~ /^[-]?\d+$/
            && $_[2] =~ /^[-]?\d+$/) {
            ($y, $m, $d) = @_
    } else {
        croak "Can't read a date from this --> [@_]"
    }
    return ($y, $m, $d);
}


sub J2G {         # returns days difference between julian on gregorian dates
    use integer;
    my ($y, $m, $d) = &_getYMD;
    # if the month is Jan or Feb then use the year before
    if ($m < 3) { $y-- }
    # the difference in leap days is just the omitted century end leap days in the
    # Gregorian calendar, less two because they didn't start until
    # some long time after 1 AD
    return $y/100 - $y/400 - 2;
}

=back

=head1 SEE ALSO

L<Date::Calc> and L<Date::Manip> packages which provide more comprehensive
functions; as they say: there's more than one way to do it.

=head1 AUTHOR

Toby Thurston

web: http://www.wildfire.dircon.co.uk

=cut

1;
