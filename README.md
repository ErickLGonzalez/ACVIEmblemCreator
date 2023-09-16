# Armored Core VI Web Emblem Creator

Armored Core 6 Emblem Tool for use in the Armored Core 6 Garage Simulator.

This tool effortlessly import your own images into ACVI to use as emblems on your Armored Core and share online. The tool leverages data from the [Geometrize library](https://github.com/Tw1ddle/geometrize-haxe) which decomposes arbitrary images into geometric primitives. This vector format is then converted to the game's native emblem format and imported into the game. It's easy to use, it's fast, give it a try!

This is a Javascript Node.js version of **[pawREP's ACVIEmblemCreator for PC](https://github.com/pawREP/ACVIEmblemCreator/releases/latest)**. 

### Legacy usage
  You can still generate emblems by vectorizing and exporting images as json from the [Geometrize Web App](https://www.samcodes.co.uk/project/geometrize-haxe-web/). You can import the jsons using the `Load Json` button in the GUI. This method might give you a better preview of the final result for shapes with low alpha. 
