# Code Formatter and Demo App binaries
The folder contains scripts, binaries and configuration files related to code formatting tools, vscode settings and release binaries for the captive portal demo, coming pre-flashed with each xplr-hpg kit.<br>

## AStyle - Code Formatter
[AStyle](http://astyle.sourceforge.net/) is a source code indenter, formatter, and beautifier that is used across u-blox repositories to provide a common coding template.<br>
For more information regarding this tool and its capabilities please have a look [here](http://astyle.sourceforge.net/astyle.html)<br>
Below is a table which (roughly) sums up what AStyle can and what cannot do for you.

| Can Do                                             | Cannot Do                                                            |
|:---------------------------------------------------|:---------------------------------------------------------------------|
| Format your code according to popular formats      | Check functions and arguments for given format (camel, snake etc.)   |
| Arrange indentation                                | Format padding when explicitly added from user                       |
| Format "one-liners"                                | Format line breaks when explicitly added from user                   |
| Format padding in arguments*                       | Spell check your code                                                |
| Break / Split lines according to predefined length | Pad columns to same length (eg lining-up MACROs, structs etc.)       |
| Adjust tab size                                    |                                                                      |
| Replace tab with space characters (and vice versa) |                                                                      |

An alternative to AStyle with more formatting options is [CLang](https://clang.llvm.org/docs/ClangFormatStyleOptions.html) but is not used in u-blox.

### VSCode integration
Using AStyle formatter within VSCode is as simple as:
* Installing [Astyle](https://marketplace.visualstudio.com/items?itemName=chiehyu.vscode-astyle) extension for VSCode.
* Copying the settings provided in [settings.json](./settings.json) to your `.vscode/settings.json` file.
* Opening a source file in the editor, doing a right click and selecting `Format Document` option or using the `Alt+Shift+F` shortcut.

**NOTE:** You can configure the vscode editor to format any modified file upon saving/compiling.<br>
To do so just uncomment the `"editor.formatOnSave": true,` line inside `.vscode/settings.json` file.

### Vim integration
To use AStyle from Vim editor just follow [this](https://blog.shichao.io/2014/01/16/artistic_style_vim.html) tutorial and update your config file with the options shown in `bin/.astylerc`.

### Batch Formatting Source Files
In order to batch format any source files (`*.c`, `*.h`) residing under [components](../components/) and [examples](../examples/) folder the [batch formatter](./astyle_batch_formatter.bat) can be used.<br>
To run the script:
* Open a command promt/powershell window.
* Navigate to `<Repository Dir>/XPLR-HPG-SW/bin`
* Run the script using `.\astyle_batch_formatter.bat`

The script supports up to 3 args to be passed in AStyle.<br> For instance you can "dry run" the script (aka check script results without actually applying the changes in source files) by adding the `==dry-run` option like:
* `.\astyle_batch_formatter.bat --dry-run`.

For more options regarding AStyle, please have a look at the [documentation](http://astyle.sourceforge.net/astyle.html#_Other_Options)

## Binaries
Pre-compiled binaries of the [Captive Portal Demo](./../examples/shortrange/05_hpg_wifi_mqtt_correction_captive_portal/) application, which comes pre-flashed with the xplr-hpg kits, are available under the [releases](./releases/) folder for all available kit variants.<br>
Instructions on how to flash them to your board can be found [here](./../docs/README_flashing_guide.md).