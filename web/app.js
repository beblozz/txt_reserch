// Слова считаются так же, как в C++ части: латиница, цифры и кириллица.
const WORD_REGEX = /[0-9A-Za-zЀ-ѿ]+/g;

function splitWords(text) {
    return (text.match(WORD_REGEX) || []).map((w) => w.toLowerCase());
}

function formatNumber(n) {
    return n.toLocaleString("ru-RU");
}

// Разбивает текст на куски и отмечает найденные слова или фразу.
function highlight(text, query, phrase) {
    const queryWords = splitWords(query);
    const tokens = [];
    for (const m of text.matchAll(WORD_REGEX)) {
        tokens.push({ word: m[0].toLowerCase(), start: m.index, end: m.index + m[0].length });
    }

    const ranges = [];
    if (phrase) {
        const n = queryWords.length;
        for (let i = 0; n > 0 && i + n <= tokens.length; i++) {
            let ok = true;
            for (let j = 0; j < n; j++) {
                if (tokens[i + j].word !== queryWords[j]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                ranges.push([tokens[i].start, tokens[i + n - 1].end]);
                i += n - 1;
            }
        }
    } else {
        const set = new Set(queryWords);
        for (const t of tokens) {
            if (set.has(t.word)) {
                ranges.push([t.start, t.end]);
            }
        }
    }

    const parts = [];
    let last = 0;
    for (const [start, end] of ranges) {
        if (start > last) {
            parts.push({ text: text.slice(last, start), mark: false });
        }
        parts.push({ text: text.slice(start, end), mark: true });
        last = end;
    }
    if (last < text.length) {
        parts.push({ text: text.slice(last), mark: false });
    }
    return parts;
}

function Parts(props) {
    return props.parts.map((part, i) =>
        part.mark ? <mark key={i}>{part.text}</mark> : <span key={i}>{part.text}</span>
    );
}

function Stats(props) {
    if (!props.stats) {
        return null;
    }
    return (
        <div className="stats">
            <div className="stat">
                <div className="stat-value">{formatNumber(props.stats.files)}</div>
                <div className="stat-label">файлов</div>
            </div>
            <div className="stat">
                <div className="stat-value">{formatNumber(props.stats.words)}</div>
                <div className="stat-label">слов</div>
            </div>
            <div className="stat">
                <div className="stat-value">{formatNumber(props.stats.unique)}</div>
                <div className="stat-label">уникальных слов</div>
            </div>
            <div className="stat">
                <div className="stat-value">{props.stats.ms.toFixed(2)} мс</div>
                <div className="stat-label">время индексации</div>
            </div>
        </div>
    );
}

function ResultItem(props) {
    const result = props.result;
    const parts = result.parts || [{ text: result.snippet, mark: false }];
    return (
        <div className="result" onClick={() => props.onOpen(result.path)} title="Открыть файл">
            <div className="result-top">
                <div className="result-path">{props.number}. {result.path}</div>
                <div className="result-count">совпадений: {result.count}</div>
            </div>
            <div className="result-snippet">
                <Parts parts={parts} />
            </div>
        </div>
    );
}

function FileView(props) {
    const [file, setFile] = React.useState(null);
    const [error, setError] = React.useState("");
    const [current, setCurrent] = React.useState(0);
    const textRef = React.useRef(null);

    React.useEffect(() => {
        fetch("/api/file?path=" + encodeURIComponent(props.path))
            .then((res) => res.json())
            .then((data) => {
                if (data.error) {
                    setError("Файл не найден. Возможно, папка была переиндексирована.");
                } else {
                    setFile(data);
                }
            })
            .catch(() => setError("Сервер не отвечает"));
    }, [props.path]);

    React.useEffect(() => {
        function onKeyDown(e) {
            if (e.key === "Escape") {
                props.onClose();
            }
        }
        document.addEventListener("keydown", onKeyDown);
        return () => document.removeEventListener("keydown", onKeyDown);
    }, []);

    const parts = React.useMemo(
        () => (file ? highlight(file.text, props.query, props.phrase) : []),
        [file, props.query, props.phrase]
    );
    const marksCount = parts.filter((p) => p.mark).length;

    React.useEffect(() => {
        if (!textRef.current) {
            return;
        }
        const marks = textRef.current.querySelectorAll("mark");
        marks.forEach((m, i) => m.classList.toggle("current", i === current));
        if (marks[current]) {
            marks[current].scrollIntoView({ block: "center" });
        }
    }, [parts, current]);

    function next(step) {
        if (marksCount > 0) {
            setCurrent((current + step + marksCount) % marksCount);
        }
    }

    return (
        <div className="modal-back" onClick={props.onClose}>
            <div className="modal" onClick={(e) => e.stopPropagation()}>
                <div className="modal-top">
                    <div className="result-path">{props.path}</div>
                    <button className="button-gray" onClick={props.onClose}>Закрыть</button>
                </div>
                {file && (
                    <div className="modal-info">
                        <span>слов: {formatNumber(file.words)}, подсвечено: {marksCount}</span>
                        {marksCount > 1 && (
                            <span className="modal-nav">
                                <button className="button-gray button-small" onClick={() => next(-1)}>↑</button>
                                <span>{current + 1} / {marksCount}</span>
                                <button className="button-gray button-small" onClick={() => next(1)}>↓</button>
                            </span>
                        )}
                    </div>
                )}
                {error !== "" && <div className="error">{error}</div>}
                {!file && error === "" && <div className="info">Загрузка...</div>}
                {file && (
                    <div className="file-text" ref={textRef}>
                        <Parts parts={parts} />
                    </div>
                )}
            </div>
        </div>
    );
}

function App() {
    const [stats, setStats] = React.useState(null);
    const [folder, setFolder] = React.useState("");
    const [query, setQuery] = React.useState("");
    const [phrase, setPhrase] = React.useState(false);
    const [top, setTop] = React.useState(10);
    const [answer, setAnswer] = React.useState(null);
    const [last, setLast] = React.useState({ query: "", phrase: false });
    const [error, setError] = React.useState("");
    const [loading, setLoading] = React.useState("");
    const [openPath, setOpenPath] = React.useState(null);

    React.useEffect(() => {
        fetch("/api/stats")
            .then((res) => res.json())
            .then((data) => {
                setStats(data);
                setFolder(data.folder);
            })
            .catch(() => setError("Сервер не отвечает. Запустите программу с ключом --web."));
    }, []);

    function search(newTop) {
        if (query.trim() === "") {
            return;
        }
        if (splitWords(query).length === 0) {
            setError("В запросе нет ни одного слова");
            return;
        }
        const url = "/api/search?q=" + encodeURIComponent(query) +
            "&phrase=" + (phrase ? "1" : "0") +
            "&top=" + (newTop !== undefined ? newTop : top);

        setLoading("search");
        fetch(url)
            .then((res) => res.json())
            .then((data) => {
                setAnswer(data);
                setLast({ query: query, phrase: phrase });
                setError("");
            })
            .catch(() => setError("Сервер не отвечает"))
            .finally(() => setLoading(""));
    }

    function reindex() {
        if (folder.trim() === "") {
            setError("Укажите папку");
            return;
        }
        setLoading("index");
        fetch("/api/index?path=" + encodeURIComponent(folder.trim()), { method: "POST" })
            .then((res) => res.json())
            .then((data) => {
                if (data.error) {
                    setError("Папка не найдена: " + folder);
                } else {
                    setStats(data);
                    setAnswer(null);
                    setError("");
                }
            })
            .catch(() => setError("Сервер не отвечает"))
            .finally(() => setLoading(""));
    }

    function changeTop(value) {
        setTop(value);
        if (answer) {
            search(value);
        }
    }

    return (
        <div className="container">
            <h1>Поиск по текстовым файлам</h1>
            <p className="subtitle">Движок на C++: обратный индекс на хеш-таблицах</p>

            <Stats stats={stats} />

            <div className="row">
                <input
                    type="text"
                    value={folder}
                    placeholder="Папка с .txt файлами"
                    onChange={(e) => setFolder(e.target.value)}
                    onKeyDown={(e) => e.key === "Enter" && reindex()}
                />
                <button className="button-gray" onClick={reindex} disabled={loading !== ""}>
                    {loading === "index" ? "Индексирую..." : "Индексировать"}
                </button>
            </div>

            <div className="row">
                <input
                    type="text"
                    value={query}
                    placeholder="Что ищем?"
                    autoFocus
                    onChange={(e) => setQuery(e.target.value)}
                    onKeyDown={(e) => e.key === "Enter" && search()}
                />
                <button onClick={() => search()} disabled={loading !== ""}>
                    {loading === "search" ? "Ищу..." : "Найти"}
                </button>
            </div>

            <div className="options">
                <label>
                    <input
                        type="checkbox"
                        checked={phrase}
                        onChange={(e) => setPhrase(e.target.checked)}
                    />
                    {" "}точная фраза
                </label>
                <label>
                    показывать топ{" "}
                    <select value={top} onChange={(e) => changeTop(Number(e.target.value))}>
                        <option value={5}>5</option>
                        <option value={10}>10</option>
                        <option value={50}>50</option>
                        <option value={0}>все</option>
                    </select>
                </label>
            </div>

            {error !== "" && <div className="error">{error}</div>}

            {answer && (
                <div>
                    <div className="info">
                        Найдено файлов: {answer.found}, время поиска: {answer.ms.toFixed(3)} мс
                        {answer.results.length < answer.found && <span>, показаны первые {answer.results.length}</span>}
                    </div>
                    {answer.found === 0 && (
                        <div className="empty">
                            По запросу «{last.query}» ничего не найдено.
                            {last.phrase ? " Попробуйте выключить «точную фразу»." : " Попробуйте убрать часть слов."}
                        </div>
                    )}
                    {answer.results.map((result, i) => (
                        <ResultItem key={result.path} number={i + 1} result={result} onOpen={setOpenPath} />
                    ))}
                </div>
            )}

            {openPath && (
                <FileView
                    path={openPath}
                    query={last.query}
                    phrase={last.phrase}
                    onClose={() => setOpenPath(null)}
                />
            )}
        </div>
    );
}

const root = ReactDOM.createRoot(document.getElementById("root"));
root.render(<App />);
