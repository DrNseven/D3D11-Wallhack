# D3D11-Wallhack
D3D11 Hook Wallhack x86/x64

How to log models:
- run the game, inject dll, press insert for menu
- 1. press F9 to see which drawing function is called by the game
- 2. select DeleteTexture
- 3. select Stride and use the slider till an enemy model/texture disappears
- 4. press END to log the values of that model/texture to log.txt
- 5. add that Stride number to your model recognition, example if(Stride == 32)
- 6. next log IndexCount of that model Stride
- 7. add IndexCount to your model rec, example if(Stride == 32 && IndexCount == 10155) etc.

Credits: dracorx, evolution536, test4321, Momo5000

Menu key = INSERT
![alt tag](https://github.com/DrNseven/D3D11-Wallhack/blob/master/menu.jpg)

Here is the old version with FW1FontWrapper and MinHook: https://github.com/DrNseven/D3D11-Wallhack/tree/acc8e259e813da2e9d7ad6a39f6fcefaf1de1800
