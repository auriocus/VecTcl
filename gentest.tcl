proc gentest {topic subject code} {
    # convenience to generate a test case 
    # for exact comparison
    set fd [open tests/nrcache r]
    set nrcache [read $fd]
    close $fd

    set returncode [catch $code result]
    set nr [dict get [dict incr nrcache $subject] $subject]
    set test "test $topic $subject-$nr -body \{\n"
    append test "\t[string map {\n \n\t} $code]\n"
    append test "\} -result [list $result]"
    if {$returncode} {
	append test " -returnCodes $returncode"
    }
    append test "\n"

    set fd [open tests/$topic.test a]
    puts $fd $test
    close $fd

    set fd [open tests/nrcache w]
    puts -nonewline $fd $nrcache
    close $fd 

    return $test
}
