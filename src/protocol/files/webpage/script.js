const asciiArt = 
"                        -==.\n" +
"                         ====-\n" +

"                         ==-====                                   --==:\n" +
"                         ==+=--===                    ==========:     -\n" +
"                         -=#*-=-===                  =====-------==    -==\n" +
"                         ==#+=+=-====--              =*#===--==+++==+. -===\n" +
"                         ==#*=+*-+=##==@@@=:         %=%%#*=+++++++*++=*%%======\n" +
"                :-=-     ===#***+*=%@%%#**%%@@@@    @%%*+++=+==*++++++#==--===\n" +
"                 =--===  #=#++*#+=--===++*++@@=@@***+**%%@%@*+=+*===*=+#\n" +
"             ---: .-=---====    %%*#***#=-+=+=*#++++*+=####%%@@#%%:-@%%%%**+=+#=+#=         :-===-\n" +
"             ========--===-==%@=:   @+#+#*-+=+++++=+**==*#---::.     :=*#*==+=#*+- =======+#*==-=\n" +
"               =#@#+---::-=====+@%%+==:@%%#*%%*==+=++**++*+--==========--:.==#*++*##-+==+=+####=---=-\n" +
"               ---@@@*-:-+-----==%%+-=@%%##*=+=+-++=++#*==:-----====-.--===#**#+===++#***=======\n" +
"                 ==--*%%%%=-*==-=++==*+..*%%##=-==-*==-===-++-=++-=---=-==--=%%*+==*==**#====..::.\n" +
"                   ===:=*#***==+*#***%%.-=@%%#===+--=====-+=*-==*=-::=-=---##+=*#-=++===#*#*====------.\n" +
"                     ====:=*##*+====*#%%=:-%%%==-++=+**==*-==+===-----=-=*#+*#++*####**=--:::::====\n" +
"                       ====:===%%+.:####%%@*:::%#+++++=+*++#+*+-+==-==+-==+%%###*##=================\n" +
"                         ====:====::--=*#@@#%@%%*****%%+*=+++-+=--===+=+=##*#=====--=--=:-=+-===.\n" +
"                           .:==-====--::::--=--====*@@%%%%#==++#%%%%%%###*====+++=+*+==*#+*==\n" +
"                ----==--.  ... ===========:=:--:======%%#+=====-----=+#%%%%*=+*==+%%%%#*\n" +
"                ========-:  .::... -*=:=------=-:=--::--==+=+-:-===-=-----:-=%%@@@%%@@\n" +
"               ====::.:======-=-:-:. =+=--:-------+--===-.=====---=-:====-==---::..-=====--\n" +
"              ===-::==------==-+***=--.-===-+=========\n" +
"             -===============-:::..:...  :--=====\n" +

"                 :--::---..:::::-:-=:::.                 =========:--====:=-===--:::--=======\n" +
"                    .-------::: .                       ====-==========:::---------\n" +
"                                                               .:--::     .::";

document.getElementById("ascii-bg").textContent = asciiArt;

const params = new URLSearchParams(window.location.search);
const id = params.get("id");

const status = document.querySelector("#status");
const fileId = document.querySelector("#file-id");
const downloadLink = document.querySelector("#download-link");

async function verifyAndSetupLink() {
	if (!id || !/^\d+$/.test(id)) {
		status.textContent = "[error]: missing a valid file id";
		status.style.color = "#f87171";
		return;
	}

	status.textContent = "[info]: verifying link with server...";
	status.style.color = "#888888";

	try {
		const response = await fetch(`/files?id=${encodeURIComponent(id)}`, { method: 'HEAD' });

		if (response.ok) {
			fileId.textContent = `[info]: file share id - ${id}`;
			fileId.hidden = false;

			downloadLink.href = `/files?id=${encodeURIComponent(id)}`;
			downloadLink.hidden = false;

			status.textContent = "[success]: link is ready to download";
			status.style.color = "#4ade80";
		} else {
			status.textContent = "[error]: file not found or link expired";
			status.style.color = "#f87171";
		}
	} catch (err) {
		status.textContent = "[error]: failed to connect to server";
		status.style.color = "#f87171";
	}
}

verifyAndSetupLink();
