(* BENCH: sieve limit=400000 reps=80 *)
let limit = 400000
let reps = 80

let () =
  let flags = Bytes.make limit '\001' in
  let sum = ref 0 in
  for _r = 1 to reps do
    for i = 0 to limit - 1 do
      Bytes.set flags i '\001'
    done;
    Bytes.set flags 0 '\000';
    Bytes.set flags 1 '\000';
    let i = ref 2 in
    while !i * !i < limit do
      if Bytes.get flags !i <> '\000' then begin
        let j = ref (!i * !i) in
        while !j < limit do
          Bytes.set flags !j '\000';
          j := !j + !i
        done
      end;
      i := !i + 1
    done;
    sum := 0;
    for k = 2 to limit - 1 do
      if Bytes.get flags k <> '\000' then sum := !sum + k
    done
  done;
  Printf.printf "%d\n" !sum
