(* BENCH: parse n=65536 reps=512 *)
let n = 65536
let reps = 512

let () =
  let buf = Bytes.make n '\000' in
  let i = ref 0 in
  while !i < n do
    if !i mod 10 = 9 then Bytes.set buf !i (Char.chr 44)
    else Bytes.set buf !i (Char.chr (48 + (!i mod 10)));
    i := !i + 1
  done;
  let r = ref 0 in
  let sum = ref 0 in
  while !r < reps do
    i := 0;
    let v = ref 0 in
    while !i < n do
      let c = Char.code (Bytes.get buf !i) in
      if c = 44 then begin
        sum := !sum + !v;
        v := 0
      end
      else v := !v * 10 + (c - 48);
      i := !i + 1
    done;
    sum := !sum + !v;
    r := !r + 1
  done;
  Printf.printf "%d\n" !sum
