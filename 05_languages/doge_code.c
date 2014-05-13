map_parser_t* Adjective = map_or(4,
                                 mpc_sym("wow"), mpc_sym("many"),
                                 mpc_sym("so"), mpc_sym("such")
                                 );

mpc_parser_t* Noun = mpc_or(5,
                            mpc_sym("lisp"), mpc_sym("language"),
                            mpc_sym("c"), mpc_sym("book"),
                            mpc_sym("build")
                            );
