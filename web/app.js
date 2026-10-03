function Stats(props) {
    if (!props.stats) {
        return null;
    }
    return (
        <div className="stats">
            <div className="stat">
                <div className="stat-value">{props.stats.files}</div>
                <div className="stat-label">файлов</div>
            </div>
            <div className="stat">
                <div className="stat-value">{props.stats.words}</div>
                <div className="stat-label">слов</div>
            </div>
            <div className="stat">
                <div className="stat-value">{props.stats.unique}</div>
                <div className="stat-label">уникальных слов</div>
            </div>
            <div className="stat">
                <div className="stat-value">{props.stats.ms.toFixed(2)} мс</div>
                <div className="stat-label">время индексации</div>
            </div>
        </div>
    );
}

function Snippet(props) {
    const queryWords = props.query.toLowerCase().split(" ");
    const words = props.text.split(" ");

    return (
        <div className="result-snippet">
            {words.map((word, i) => {
                if (queryWords.includes(word.toLowerCase())) {
                    return <span key={i}><mark>{word}</mark> </span>;
                }
                return <span key={i}>{word} </span>;
            })}
        </div>
    );
}

function ResultItem(props) {
    return (
        <div className="result">
            <div className="result-top">
                <div className="result-path">{props.number}. {props.result.path}</div>
                <div className="result-count">совпадений: {props.result.count}</div>
            </div>
            <Snippet text={props.result.snippet} query={props.query} />
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
    const [lastQuery, setLastQuery] = React.useState("");
    const [error, setError] = React.useState("");

    React.useEffect(() => {
        fetch("/api/stats")
            .then((res) => res.json())
            .then((data) => {
                setStats(data);
                setFolder(data.folder);
            });
    }, []);

    function search() {
        if (query.trim() === "") {
            return;
        }
        const url = "/api/search?q=" + encodeURIComponent(query) +
            "&phrase=" + (phrase ? "1" : "0") +
            "&top=" + top;

        fetch(url)
            .then((res) => res.json())
            .then((data) => {
                setAnswer(data);
                setLastQuery(query);
                setError("");
            })
            .catch(() => setError("Сервер не отвечает"));
    }

    function reindex() {
        fetch("/api/index?path=" + encodeURIComponent(folder), { method: "POST" })
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
            .catch(() => setError("Сервер не отвечает"));
    }

    function onKeyDown(e) {
        if (e.key === "Enter") {
            search();
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
                />
                <button className="button-gray" onClick={reindex}>Индексировать</button>
            </div>

            <div className="row">
                <input
                    type="text"
                    value={query}
                    placeholder="Что ищем?"
                    onChange={(e) => setQuery(e.target.value)}
                    onKeyDown={onKeyDown}
                />
                <button onClick={search}>Найти</button>
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
                    <select value={top} onChange={(e) => setTop(Number(e.target.value))}>
                        <option value={5}>5</option>
                        <option value={10}>10</option>
                        <option value={50}>50</option>
                    </select>
                </label>
            </div>

            {error !== "" && <div className="error">{error}</div>}

            {answer && (
                <div>
                    <div className="info">
                        Найдено файлов: {answer.found}, время поиска: {answer.ms.toFixed(3)} мс
                    </div>
                    {answer.results.map((result, i) => (
                        <ResultItem key={result.path} number={i + 1} result={result} query={lastQuery} />
                    ))}
                </div>
            )}
        </div>
    );
}

const root = ReactDOM.createRoot(document.getElementById("root"));
root.render(<App />);
