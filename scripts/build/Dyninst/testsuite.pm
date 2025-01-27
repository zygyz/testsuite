package Dyninst::testsuite;

use base 'Exporter';
our @EXPORT_OK = qw(setup configure build run);

use Dyninst::utils qw(execute list_unique load_from_cache);
use Dyninst::git;
use Cwd qw(realpath);
use File::Path qw(make_path);


sub setup {
	my ($root_dir, $args, $fdLog) = @_;
	
	# Create the build directory
	make_path("$root_dir/testsuite/build");

	my $base_dir = realpath("$root_dir/testsuite");
	my $build_dir = "$base_dir/build";
	symlink($args->{'test-src'}, "$base_dir/src");
	symlink(realpath("$root_dir/dyninst"), "$base_dir/dyninst");
	
	# This is for internal use only
	$args->{'testsuite-cmake-cache-dir'} = $build_dir;
	
	my $git_config = Dyninst::git::get_config($args->{'test-src'}, $base_dir);
	
	# Check out the PR, if specified
	if($args->{'testsuite-pr'}) {
		Dyninst::git::checkout_pr($args->{'test-src'}, $args->{'testsuite-pr'}, $git_config->{'branch'});
		$git_config = Dyninst::git::get_config($args->{'test-src'}, $base_dir);
	}
	
	Dyninst::git::save_config($base_dir, $git_config->{'branch'},$git_config->{'commit'});
	
	return ($base_dir, $build_dir);
}


sub configure {
	my ($args, $base_dir, $build_dir) = @_;
	
	# Configure the Testsuite
	# We need an 'eval' here since we are manually piping stderr
	eval {
		execute(
			"cd $build_dir\n" .
			"cmake ../src -DCMAKE_INSTALL_PREFIX=$base_dir " .
			"$args->{'cmake-args'} " .
			"$args->{'testsuite-cmake-args'} " .
			"-DINSTALL_DIR=$base_dir/tests ".
			"-DDyninst_DIR=../dyninst/lib/cmake/Dyninst ".
			"1>config.out 2>config.err"
		);
	};
	die "Error configuring: see $build_dir/config.err for details" if $@;
}

sub build {
	my ($args, $build_dir) = @_;

	my $njobs = $args->{'njobs'};

	# Build the Testsuite
	# We need an 'eval' here since we are manually piping stderr
	eval {
		execute(
			"cd $build_dir\n" .
			"make VERBOSE=1 -j$njobs 1>build.out 2>build.err"
		);
	};
	die "Error building: see $build_dir/build.err for details" if $@;

	# Install the Testsuite
	# We need an 'eval' here since we are manually piping stderr
	eval {
		execute(
		"cd $build_dir\n" .
		"make VERBOSE=1 install 1>build-install.out 2>build-install.err"
		);
	};
	die "Error installing: see $build_dir/build-install.err for details" if $@;
}
sub run {
	my ($args, $base_dir) = @_;

	# Grab the paths in the Dyninst build cache
	my @lib_dirs = (
		'Boost_LIBRARY_DIRS','TBB_LIBRARY_DIRS','ElfUtils_LIBRARY_DIRS',
		'LibIberty_LIBRARY_DIRS'
	);
	my $cache = "$args->{'dyninst-cmake-cache-dir'}/CMakeCache.txt";
	my @libs = load_from_cache($cache, \@lib_dirs);
	
	# Grab the paths in the test suite build cache
	$cache = "$args->{'testsuite-cmake-cache-dir'}/CMakeCache.txt";
	@lib_dirs = 'LibXml2_LIBRARY_DIRS';
	push @libs, load_from_cache($cache, \@lib_dirs);
	
	push @libs, ($base_dir, realpath("$base_dir/../dyninst/lib"));
	my $paths = join(':', list_unique(@libs));

	my $err = undef;
	# We need an 'eval' here since we are manually piping stderr
	eval {
		execute(
			"cd $base_dir\n" .
			"export DYNINSTAPI_RT_LIB=$base_dir/../dyninst/lib/libdyninstAPI_RT.so\n".
			"LD_LIBRARY_PATH=$paths:\$LD_LIBRARY_PATH " .
			"./runTests -all -log test.log -j$args->{'ntestjobs'} 1>stdout.log 2>stderr.log"
		);
	};
	$err = $@ if $@;

	# Check if we were killed by the watchdog timer
	open my $fdIn, '<', "$base_dir/stderr.log";
	while(<$fdIn>) {
		return !!0 if m/Process exceeded time limit/;
	}

	# The run failed for some reason other than the watchdog timer
	chomp($err);
	if($err && $err eq '') {
		# runTest returned a non-zero value, but no actual error
		# message. Check the log for an error message
		open my $fdIn, '<', "$base_dir/stderr.log" or die "$!\n";
		while(<$fdIn>) {
			if(/\berror\b/i) {
				die "\nrunTests terminated abnormally\n\n\n$err\n";
			}
		}
	}
	
	return !!1;
}

1;