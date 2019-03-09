# D3D11-Wallhack
D3D11 Hook Wallhack x86/x64

1. Compile dll and inject into a d3d11 game
2. Find models by brute-forcing Stride or IndexCount etc. see universal.cpp 
3. Brute-force the correct depth of those models (countEdepth and countRdepth)

- apex: Edepth = 1; Rdepth = 3; 
- ut4: Edepth = 11; Rdepth = 12;
- serious sam fus: Edepth = 11; Rdepth = 4;
- quake c: Edepth = 0; Rdepth = 2;

Credits: dracorx, evolution536, test4321, Momo5000

Menu key = INSERT
![alt tag](https://github.com/DrNseven/D3D11-Wallhack/blob/master/d3d11wallhack.jpg)

Here is the old version with FW1FontWrapper and MinHook: https://github.com/DrNseven/D3D11-Wallhack/tree/acc8e259e813da2e9d7ad6a39f6fcefaf1de1800
