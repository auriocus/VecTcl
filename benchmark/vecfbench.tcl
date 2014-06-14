
# vecpde.tcl --
#     Solve a simple PDE problem:
#     A bar is heated on one side with the other side isolated
#
lappend auto_path . /usr/lib
package require vectcl
namespace import vectcl::*

set compiler gfortran-mp-4.6
set OPT {-march=core2 -O3}

# The coefficients
#
vexpr {
    k     = 1.0e-3   # m2/s - thermal conductivity
    deltt = 0.01     # s - time step
    deltx = 0.01     # m - size of grid cell

    alpha = k * deltt / deltx^2
}

proc report {i t0 t1 t2 t3 t4 t5} {
	puts [format "%10d%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f" $i $t0 $t1 $t2 $t3 $t4 $t5]
}
	

#
# Time loop

# cut down function calls by eliminating constants
# only local vars
vproc diffusion {alpha ngrid} {
	temp  = constfill(0.0,ngrid+1)
	dtemp = constfill(0.0,ngrid+1)

	temp[ngrid] = 1.0 # The right-hand boundary condition: constant temperature

	n1 = 1*ngrid/4
	n2 = 2*ngrid/4
	n3 = 3*ngrid/4
	n4 = 7*ngrid/8

	for i=0:100000-1 {
		#dtemp[1:ngrid-1] = alpha * (temp[0:ngrid-2] + temp[2:ngrid] - 2.0 * temp[1:ngrid-1])
		#temp = temp + dtemp

		# do an in-place update instead
		temp[1:-2] += alpha * (temp[0:-3] + temp[2:-1] - 2.0 * temp[1:-2])

		# Boundary condition on the left side: zero flux

		temp[0] = temp[1]

		if i%5000 == 0 {
			temp_0  = temp[1]
			temp_1  = temp[n1]
			temp_2  = temp[n2]
			temp_3  = temp[n3]
			temp_4  = temp[n4]
			temp_5  = temp[-2]

			#report(i,temp_0,temp_1,temp_2,temp_3,temp_4,temp_5)
		}
	}
}

proc diffusion_fortran {ngrid} {
	set fd [open temp.f90 w]
	puts $fd [subst -nocommands {

! pde.f90 --
!     Quick and dirty implementation of the PDE problem shown in vecpde.tcl using Fortran
!
program vecpde
    implicit none

    integer, parameter :: dp = kind(1.0d0) ! Make it all double-precision
    integer, parameter :: ngrid = $ngrid
    real(kind=dp), dimension(ngrid+1) :: temp
    real(kind=dp), dimension(ngrid+1) :: dtemp
    real(kind=dp)                     :: k, deltt, deltx, alpha
    real(kind=dp)                     :: temp_0, temp_1, temp_2, temp_3, temp_4, temp_5
    integer                           :: i

    integer                           :: count, count_rate, start

    !
    ! Initialisation
    !
    k     = 1.0e-3_dp ! m2/s
    deltt = 0.01_dp   ! m
    deltx = 0.01_dp   ! s

    alpha = k * deltt / deltx ** 2

    temp  = 0.0_dp
    dtemp = 0.0_dp

    !
    ! Boundary condition
    !
    temp(ngrid+1) = 1.0_dp

    call system_clock( start, count_rate )

    do i = 1,100000
        dtemp(2:ngrid) = alpha * ( temp(1:ngrid-1) + temp(3:ngrid+1) - 2.0_dp * temp(2:ngrid) )
        temp           = temp + dtemp

        ! Boundary condition on the left: zero flux
        temp(1)        = temp(2)

        ! Slightly different locations because of start at 1 instead of 0

        temp_0         = temp(2)
        temp_1         = temp(1*ngrid/4)
        temp_2         = temp(2*ngrid/4)
        temp_3         = temp(3*ngrid/4)
        temp_4         = temp(7*ngrid/8)
        temp_5         = temp(ngrid-2)

        if ( mod(i, 5000) == 1 ) then
            write(*,'(i10,6f10.4)') i, temp_0, temp_1, temp_2, temp_3, temp_4, temp_5
        endif
    enddo

    call system_clock( count, count_rate )

    write(*,*) 'Time: ', dble(count-start)/count_rate
end program

	}]
	close $fd

	exec $::compiler {*}$::OPT -o ./temp temp.f90
	set result [exec ./temp]
	# last number is the time in seconds w/o compilation and starting the process
	set s [lindex $result end] ;# very lazy, but works
	return [expr {$s*1e6}] ;# return microseconds
}

# run benchmark
set gridsize {5 10 20 50 100 200 500 1000 2000 5000 10000}
#set gridsize {5 10}
foreach ngrid $gridsize {
	set t [time {diffusion $alpha $ngrid}]
	lappend tcl [lindex $t 0] ;# microseconds
	lappend fortran [diffusion_fortran $ngrid]
}

# output result
set fd [open fbench.dat w]
foreach t $tcl f $fortran n $gridsize {
	puts $fd "$n $t $f"
}
close $fd
