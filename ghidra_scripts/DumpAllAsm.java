// Writes the disassembly of every non-thunk function.
// args: <out file>
//@category Trainz

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class DumpAllAsm extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
			FunctionIterator funcs = currentProgram.getFunctionManager().getFunctions(true);
			while (funcs.hasNext() && !monitor.isCancelled()) {
				Function f = funcs.next();
				if (f.isThunk() || f.isExternal()) continue;
				w.printf("%n; ==== %s %s%n", f.getEntryPoint(), f.getName(true));
				AddressSetView body = f.getBody();
				InstructionIterator it = currentProgram.getListing().getInstructions(body, true);
				while (it.hasNext()) {
					Instruction ins = it.next();
					StringBuilder hex = new StringBuilder();
					for (byte b : ins.getBytes()) hex.append(String.format("%02x", b));
					StringBuilder note = new StringBuilder();
					for (ghidra.program.model.symbol.Reference r : ins.getReferencesFrom()) {
						ghidra.program.model.listing.Data d = getDataAt(r.getToAddress());
						if (d != null && d.hasStringValue()) {
							note.append(" \"").append(d.getDefaultValueRepresentation()).append('"');
						} else {
							ghidra.program.model.symbol.Symbol s = getSymbolAt(r.getToAddress());
							if (s != null) note.append(" <").append(s.getName(true)).append('>');
						}
					}
					w.printf("%s  %-20s %s%s%n", ins.getAddress(), hex, ins, note.length() > 0 ? "   ;" + note : "");
				}
			}
		}
		println("ASMDUMP done");
	}
}
