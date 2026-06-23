```
projectRoot/
├── instance/
│   ├── case/         # 版图数据
│   │   ├── Public/   # 包含一个小版图和一个大版图数据
│   │   └── Hidden/   # 包含一个版图数据
│   └── Rule/         # 包含小版图的3个 rule 和大版图的3个 rule（每个 rule 对应一个题目）
├── answer/           # 包含每个 rule 对应的 golden 输出
│   ├── Public/
│   └── Hidden/
├── solution/         # 包含每个 rule 对应的输出结果（需要与 answer 中提供的输出进行比较）
│   ├── Public/
│   └── Hidden/
└── checker/          # 判题程序
    ├── checker
    └── AnswerChecker
```

使用命令为

``` bash
checker -ans ansFile -res resFile
```

> ans File为答案文件路径，res File为待判断的文件路径，运行前需要将checker和AnswerChecker这两个文件设置755权限。

``` bash
chmod 755 checker
chmod 755 AnswerChecker
```