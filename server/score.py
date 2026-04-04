import abc
import csv
import logging
import re
import time
from dataclasses import dataclass
from typing import Optional, Tuple, cast

import requests
import socketio

log = logging.getLogger("scoreboard")


@dataclass(frozen=True)
class UIDSubmissionResult:
    uid: str
    score: int
    time_remaining: float
    message: str


class Scoreboard(abc.ABC):
    """
    The Scoreboard class connects to the server socket and enables updating score by sending UID.
    """

    @abc.abstractmethod
    def add_UID(self, UID_str: str) -> Tuple[int, float]:
        """Send {UID_str} to server to update score. Returns (score, time_remaining)."""
        pass

    @abc.abstractmethod
    def get_current_score(self) -> Optional[int]:
        """Fetch current score from server. Returns current score."""
        pass

    def submit_UID(self, UID_str: str) -> UIDSubmissionResult:
        score, time_remaining = self.add_UID(UID_str)
        return UIDSubmissionResult(
            uid=UID_str,
            score=score,
            time_remaining=time_remaining,
            message="",
        )


class ScoreboardFake(Scoreboard):
    """
    Fake scoreboard. Check uid with fakeUID.csv
    """

    def __init__(self, teamname, filepath, game_duration: float = 70.0):
        self.total_score = 0
        self.team = teamname
        self.game_duration = float(game_duration)
        self.start_time = time.monotonic()
        log.info(f"Using fake scoreboard!")
        log.info(f"{self.team} is playing game!")

        self._read_UID_file(filepath)

        self.visit_list = set()

    def _time_remaining(self) -> float:
        elapsed = time.monotonic() - self.start_time
        return max(0.0, round(self.game_duration - elapsed, 1))

    def _read_UID_file(self, filepath: str):
        self.uid_to_score = {}
        with open(filepath, "r") as f:
            reader = csv.reader(f)
            rows = list(reader)
            for row in rows[1:]:
                uid, score = row
                self.uid_to_score[uid] = int(score)
        log.info("Successfully read the UID file!")

    def submit_UID(self, UID_str: str) -> UIDSubmissionResult:
        log.debug(f"Adding UID: {UID_str}")

        if not isinstance(UID_str, str):
            raise ValueError(f"UID format error! (expected: str) (got: {UID_str})")

        if not re.match(r"^[0-9A-Fa-f]{8}$", UID_str):
            raise ValueError(
                f"UID format error! (expected: 8 hex digits) (got: {UID_str})"
            )

        if UID_str not in self.uid_to_score:
            message = f"Invalid UID {UID_str}"
            log.info(message)
            return UIDSubmissionResult(
                uid=UID_str,
                score=0,
                time_remaining=self._time_remaining(),
                message=message,
            )
        elif UID_str in self.visit_list:
            message = f"UID {UID_str} already added"
            log.info(message)
            return UIDSubmissionResult(
                uid=UID_str,
                score=0,
                time_remaining=self._time_remaining(),
                message=message,
            )
        else:
            point = self.uid_to_score[UID_str]
            self.total_score += point
            self.visit_list.add(UID_str)
            time_remaining = self._time_remaining()
            message = f"Added {point} Points at {time_remaining} seconds left."
            log.info(message)
            return UIDSubmissionResult(
                uid=UID_str,
                score=point,
                time_remaining=time_remaining,
                message=message,
            )

    def add_UID(self, UID_str: str) -> Tuple[int, float]:
        result = self.submit_UID(UID_str)
        return result.score, result.time_remaining

    def get_current_score(self):
        return int(self.total_score)


class ScoreboardServer(Scoreboard):
    """
    The Scoreboard class connects to the server socket and enables updating score by sending UID.
    """

    def __init__(self, teamname: str, host=f"http://localhost:3000", debug=False):
        self.teamname = teamname
        self.ip = host

        log.info(f"{self.teamname} wants to play!")
        log.info(f"connecting to server......{self.ip}")

        # create socket.io instance and connect to server
        self.socket = socketio.Client(logger=debug, engineio_logger=debug)
        self.socket.register_namespace(TeamNamespace("/team"))
        self.socket.connect(self.ip, socketio_path="scoreboard.io")
        self.sid = self.socket.get_sid(namespace="/team")

        # start game
        log.info("Game is starting.....")
        self._start_game(self.teamname)

    def _start_game(self, teamname: str):
        payload = {"teamname": teamname}
        res = self.socket.call("start_game", payload, namespace="/team")
        log.info(res)

    def submit_UID(self, UID_str: str) -> UIDSubmissionResult:
        """Send {UID_str} to server and return the full response payload."""
        log.debug(f"Adding UID: {UID_str}")

        if not isinstance(UID_str, str):
            raise ValueError(f"UID format error! (expected: str) (got: {UID_str})")

        if not re.match(r"^[0-9A-Fa-f]{8}$", UID_str):
            raise ValueError(
                f"UID format error! (expected: 8 hex digits) (got: {UID_str})"
            )

        res = self.socket.call("add_UID", UID_str, namespace="/team")
        if not res:
            log.error("Failed to add UID")
            return UIDSubmissionResult(
                uid=UID_str,
                score=0,
                time_remaining=0,
                message="Failed to add UID",
            )
        res = cast(dict, res)
        message = res.get("message", "No message")
        score = res.get("score", 0)
        time_remaining = res.get("time_remaining", 0)
        log.info(message)
        return UIDSubmissionResult(
            uid=UID_str,
            score=int(score),
            time_remaining=float(time_remaining),
            message=str(message),
        )

    def add_UID(self, UID_str: str) -> Tuple[int, float]:
        result = self.submit_UID(UID_str)
        return result.score, result.time_remaining

    def get_current_score(self) -> Optional[int]:
        try:
            log.debug(f"{self.ip}/current_score?sid={self.sid}")
            res = requests.get(self.ip + "/current_score", params={"sid": self.sid})
            return res.json()["current_score"]
        except Exception as e:
            log.error(f"Failed to fetch current score: {e}")
            return None


class TeamNamespace(socketio.ClientNamespace):
    def on_connect(self):
        client = cast(socketio.Client, self.client)
        log.info(f"Connected with sid {client.get_sid(namespace='/team')}")

    def on_UID_added(self, message):
        log.info(message)

    def on_disconnect(self):
        log.info("Disconnected from server")


if __name__ == "__main__":
    import time

    logging.basicConfig(level=logging.DEBUG)

    try:
        scoreboard = ScoreboardServer("WED3", "http://140.112.175.18")
        # scoreboard = ScoreboardFake("TeamName", "data/fakeUID.csv")
        time.sleep(1)

        score, time_remaining = scoreboard.add_UID("10BA617E")
        current_score = scoreboard.get_current_score()
        log.info(f"Current score: {current_score}")
        time.sleep(1)

        score, time_remaining = scoreboard.add_UID("556D04D6")
        current_score = scoreboard.get_current_score()
        log.info(f"Current score: {current_score}")
        time.sleep(1)

        score, time_remaining = scoreboard.add_UID("12345678")
        current_score = scoreboard.get_current_score()
        log.info(f"Current score: {current_score}")

    except KeyboardInterrupt:
        exit(1)
