# RPT Line Counter

Max RPT size (~4GB)

Very simple Multi-threaded application counts the most commonly spammed lines in our local server's RPT file. (at this time, more maybe added as time progresses).
Lines that aren't deemed common enough, or unique are saved to an output file.

Input file currently hardcoded to: c:/in.rpt
Output file currently hardcoded to: c:/in.rpt.txt

--

More cores = faster. 100% max (bogs down your pc unless you restrict cores using affinity in task manager) core utilization.



### Usage:
Put your giant rpt file to your C:/ and rename to "in.rpt"
Launch RPTLineCounter(_x64).exe in Administrator mode.
Press Enter.
Open C:/in.rpt.txt in your favourite text editor.