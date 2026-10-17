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
const nonce = params.get("nonce");
const status = document.querySelector("#status");
const fileId = document.querySelector("#file-id");
const downloadLink = document.querySelector("#download-link");

async function verifyAndSetupLink() {
	if (!id || !/^\d+$/.test(id) || !nonce || !/^[0-9a-f]{32}$/i.test(nonce)) {
		status.textContent = "[error]: missing a valid file id or nonce";
		status.style.color = "#f87171";
		return;
	}

	status.textContent = "[info]: verifying link with server...";
	status.style.color = "#888888";
	const q = `id=${encodeURIComponent(id)}&nonce=${encodeURIComponent(nonce)}`;

	try {
		const response = await fetch(`/files?${q}`, { method: "HEAD" });

		if (response.ok) {
			fileId.textContent = `[info]: file share id - ${id}`;
			fileId.hidden = false;

			downloadLink.href = `/files?${q}`;
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
