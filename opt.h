// SPDX-License-Identifier: MIT

#define OPTERR \
	usage(1)

#define OPTBEGIN(argc, argv) \
	while ((argc) > 0) { \
		++(argv), --(argc); \
		if ((argc) == 0 || (*(argv))[0] != '-') \
			break; \
		if ((*(argv))[1] == '-' && !(*(argv))[2]) { \
			++(argv), --(argc); \
			break; \
		} \
		for (char *_opt = &(*(argv))[1], _done = 0; !_done && *_opt; ++_opt) { \
			switch (*_opt)

#define OPTEND \
		} \
	}

#define OPTARG \
	(_done = 1, *++_opt ? _opt : argv[1] ? --(argc), *++(argv) : (OPTERR, abort(), (char *)0))
