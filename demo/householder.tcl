package require vectcl
namespace import vectcl::*

# computing Householder vector
# Golub & Loan 5.1.1

vproc house {x} {
	n=rows(x)
	if (n==1) {
		# a scalar. Just return 1 for v
		# and 0 for beta
		return(list(1.0,2.0))
	}
	sigma=x[1:-1]'*x[1:-1]
	v=x; v[0]=1
	if sigma==0 {
		beta=0
	} else {
		mu = sqrt(x[0]^2+sigma)
		if x[0]<=0 {
			v[0]=x[0]-mu
		} else {
			v[0] = -sigma / (x[0]+mu)
		}
		beta = 2*v[0]^2 / (sigma+v[0]^2)
		v = v / v[0]
	}
	list(v, beta)
}

# computing Householder QR
# Golub & Loan 5.2.1

vproc houseQR {A} {
	m, n=shape(A)
	for j=0:n-1 {
		v, beta = house(A[j:-1, j])
		# update can be done by subtracting
		w=beta*A[j:-1, j:n-1]'*v
		A[j:-1, j:n-1] -= v*w'
		if j<m-1 {
			A[j+1:-1, j]=v[1:m-j-1]
		}
		#mformat(A)
	}
	A
}

proc mformat {m} {
	# print out a matrix 
	puts "{{[join $m "}\n {"]}}"
}
