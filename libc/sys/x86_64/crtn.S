.section .init
	// Gcc will put the contents of crtend.o's .init section here
	popq %rbp
	ret

.section .fini
	// Gcc will put the contents of crtend.o's .fini section here
	popq %rbp
	ret
