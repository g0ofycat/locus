#include <stdio.h>
#include "../../src/client/client.h"


#define PINK "\033[95m"
#define LIGHT_PINK "\x1b[38;5;225m"
#define YELLOW "\033[93m"
#define GREY "\033[37m"
#define RESET "\033[0m"

/// @brief Print usage
/// @param prog: argv[0]
static void usage(const char *prog) {
	SetConsoleOutputCP(65001);

    printf("%s", LIGHT_PINK
"                                                  -==.\n"
"                                                   ====-\n"
"                                                   ==-====                               --==:\n"
"                                                   ==+=--===                       .==========:      -\n"
"                                                   -=#*-=-===                    =====-------==    -==\n"
"                                                    ==#+=+=-====--             =*#===--==+++==+. -===\n"
"                                                    ==#*=+*-+=##==@@@=:      =%%#*=+++++++*++=*%%======\n"
"                                 :-=-               ===#***+*=%@%%#**%%@@@@   @%%*+++=+==*++++++#==--===\n"
"                                   =--===            #=#++*#+=--===++*++@@=@@***+**%%@%@*+=+*===*=+#\n"
"                              ---: .-=---====        %%*#***#=-+=+=*#++++*+=####%%@@#%%:-@%%%%**+=+#=+#=          :-===-\n"
"                             ========--===-==%@=:     @+#+#*-+=+++++=+**==*#---::.     :=*#*==+=#*+- =======+#*==-=\n"
"                               =#@#+---::-=====+@%%+==:@%%#*%%*==+=++**++*+--==========--:.==#*++*##-+==+=+####=---=-\n"
"                                ---@@@*-:-+-----==%%+-=@%%##*=+=+-++=++#*==:-----====-.--===#**#+===++#***=======\n"
"                                  ==--*%%%%=-*==-=++==*+..*%%##=-==-*==-===-++-=++-=---=-==--=%%*+==*==**#====..::.\n"
"                                    ===:=*#***==+*#***%%.-=@%%#===+--=====-+=*-==*=-::=-=---##+=*#-=++===#*#*====------.\n"
"                                      ====:=*##*+====*#%%=:-%%%==-++=+**==*-==+===-----=-=*#+*#++*####**=--:::::====\n"
"                                        ====:==%%+.:####%%@*:::%#+++++=+*++#+*+-+==-==+-==+%%###*##=================\n"
"                                          ====:===::--=*#@@#%@%%*****%%+*=+++-+=--===+=+=##*#=====--=--=:-=+-===.\n"
"                                            .:==-====--::::--=--====*@@%%%%#==++#%%%%%%###*====+++=+*+==*#+*==\n"
"                                    ----==--.  ... ===========:=:--:======%%#+=====-----=+#%%%%*=+*==+%%%%#*\n"
"                                 ========-:  .::... -*=:=------=-:=--::--==+=+-:-===-=-----:-=%%@@@%%@@\n"
"                                ====::.:======-=-:-:. =+=--:-------+--===-.=====---=-:====-==---::..-=====--\n"
"                               ===-::==------==-+***=--.-===-+=========\n"
"                              -===============-:::..:...  :--=====\n"
"                                    :--::---..:::::-:-=:::.                    =========:--====:=-===--:::--=======-\n"
"                                           .-------::: .                          =====-==========:::---------\n"
"                                                                                      .:--::      .::\n"
    RESET);
	
    printf(LIGHT_PINK "\n\n"
"							 ██╗      ██████╗  ██████╗██╗   ██╗███████╗\n"
"							 ██║     ██╔═══██╗██╔════╝██║   ██║██╔════╝\n"
"							 ██║     ██║   ██║██║     ██║   ██║███████╗\n"
"							 ██║     ██║   ██║██║     ██║   ██║╚════██║\n"
"							 ███████╗╚██████╔╝╚██████╗╚██████╔╝███████║\n"
"							 ╚══════╝ ╚═════╝  ╚═════╝ ╚═════╝ ╚══════╝\n"
    RESET);

	printf(YELLOW
"\n\n								a terminal based chat-room:\n\n"
RESET);

	fprintf(stderr, PINK "					- usage: %s <host> <username> [port]\n" RESET, prog);
	fprintf(stderr, GREY "						- host: server IP address\n" RESET);
	fprintf(stderr, GREY "						- username: desired username\n" RESET);
	fprintf(stderr, GREY "						- port: server port (default: %d)\n\n" RESET, SERVER_DEFAULT_PORT);

	fprintf(stderr, PINK "					- chat commands:\n" RESET);
	fprintf(stderr, GREY "						- rename username: /rename <new_name>\n" RESET);
	fprintf(stderr, GREY "						- list current clients in port: /users\n" RESET);
	fprintf(stderr, GREY "						- reply to message: /reply <msg_id> reply_msg\n" RESET);
	fprintf(stderr, GREY "						- leave current chat: /quit\n\n" RESET);

	fprintf(stderr, PINK "					- chat formatting:\n" RESET);
	fprintf(stderr, GREY "						- * = italic\n" RESET);
	fprintf(stderr, GREY "						- ** = bold\n" RESET);
	fprintf(stderr, GREY "						- __ = underline\n" RESET);
	fprintf(stderr, GREY "						- ~~ = strikethrough\n" RESET);
}

/// @brief Run
int main(int argc, char **argv) {
	if (argc < 3) {
		usage(argv[0]);
		return 1;
	}

	const char *host = argv[1];
	const char *username = argv[2];
	uint16_t port = argc > 3 ? (uint16_t)atoi(argv[3]) : SERVER_DEFAULT_PORT;

	if (port == 0) {
		fprintf(stderr, "[client]: invalid port: %s\n", argv[3]);
		return 1;
	}

	client_state_t c;
	if (client_connect(&c, host, port, username) != MSG_OK)
		return 1;

	client_run(&c);
	client_disconnect(&c);
	return 0;
}
