// spell コンパイラの CLI（main.c の移植）
use spell::{Cx, Flow};
use std::io::Write;

fn run() -> Result<(), Flow> {
    let args: Vec<Vec<u8>> = std::env::args_os()
        .map(|a| a.as_encoded_bytes().to_vec())
        .collect();
    let mut cx = Cx::new();
    let mut only_assembly = false;
    let mut output_name: Vec<u8> = b"a.out".to_vec();
    let mut input_name: Option<Vec<u8>> = None;
    let mut i = 1;
    while i < args.len() {
        let a = &args[i];
        if a == b"-S" {
            only_assembly = true;
        } else if a == b"-o" {
            i += 1;
            // C では argv の末尾を超えると NULL になり、%s で "(null)" と出る
            output_name = args.get(i).cloned().unwrap_or_else(|| b"(null)".to_vec());
        } else {
            input_name = Some(a.clone());
        }
        i += 1;
    }
    let input_name = match input_name {
        Some(n) => n,
        None => {
            cx.err_plain("入力ファイルが指定されていません".as_bytes().to_vec())?;
            return Ok(());
        }
    };
    let prog = cx.load_all(&input_name)?;
    cx.desugar(prog);
    cx.typecheck(prog)?;

    let mut assembly_name = output_name.clone();
    assembly_name.extend_from_slice(b".s");
    let file = match std::fs::File::create(String::from_utf8_lossy(&assembly_name).into_owned()) {
        Ok(f) => f,
        Err(_) => {
            let msg = spell::msg(&[
                "ファイル`".as_bytes(),
                &assembly_name,
                "`を開けませんでした".as_bytes(),
            ]);
            cx.err_plain(msg)?;
            return Ok(());
        }
    };
    cx.asm_out = Some(std::io::BufWriter::new(file));
    cx.emit_fmt(format_args!(".intel_syntax noprefix\n"));
    cx.codegen(prog)?;
    cx.emit_fmt(format_args!(".section .note.GNU-stack,\"\",@progbits\n"));
    if let Some(w) = cx.asm_out.as_mut() {
        let _ = w.flush();
    }
    cx.asm_out = None;

    if !only_assembly {
        let cmd = format!(
            "cc -o \"{}\" \"{}\"",
            String::from_utf8_lossy(&output_name),
            String::from_utf8_lossy(&assembly_name)
        );
        let status = std::process::Command::new("sh")
            .arg("-c")
            .arg(&cmd)
            .status();
        let ok = matches!(status, Ok(s) if s.success());
        if !ok {
            cx.err_plain("実行ファイルを作成できませんでした".as_bytes().to_vec())?;
        }
    }
    Ok(())
}

fn main() {
    match run() {
        Ok(()) => {}
        // CLI モードではエラーは err_* の中で exit(1) 済みのはず。
        // ここへ来るのは想定外なので、念のため 1 で終わる
        Err(_) => std::process::exit(1),
    }
    let _ = std::io::stdout().flush();
}
