(* BENCH: sort n=28000 *)
let n = 28000

let () =
  let a = Array.make n 0 in
  for i = 0 to n - 1 do
    a.(i) <- (i * 97 + 13) land 4095
  done;
  for i = 1 to n - 1 do
    let key = a.(i) in
    let j = ref (i - 1) in
    while !j >= 0 && a.(!j) > key do
      a.(!j + 1) <- a.(!j);
      j := !j - 1
    done;
    a.(!j + 1) <- key
  done;
  let sum = ref 0 in
  for i = 0 to n - 1 do
    sum := !sum + a.(i) * (i + 1)
  done;
  Printf.printf "%d\n" !sum
