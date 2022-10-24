# CECleanExtract

[Cheat Engine][ce] 是一个游戏作弊开发环境，也可以用来辅助逆向分析。

自版本 7 开始，官网不再提供离线安装包。这个小程序就是用来提取真实下载地址。

注意这个下载后的安装包还是会有广告，但是你可以使用 [inno extract][ie] 来提取绿色版。

已经提取好的下载地址可以在[自动部署的 GitHub Pages][gh_pages] 获取。

## 提取绿色版

```sh
innoextract.exe --extract CheatEngine74.exe --output-dir ce74
```

然后就可以在 `ce74/app` 目录得到一个没有广告的二进制文件。

## 原理

启动器运行带广告的 Cheat Engine 安装程序并注入内存扫描模块。

这个内存扫描模块寻找到离线下载链接后，会通知主启动器并退出安装程序。

主启动器打印地址到终端，可以被脚本获取。

## TODO

重新打包并部署到 GitHub Pages？

[gh_pages]: https://jixunmoe.github.io/CECleanInstallerExtract/
[ce]: https://github.com/cheat-engine/cheat-engine
[ie]: https://constexpr.org/innoextract/
